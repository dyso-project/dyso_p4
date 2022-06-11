#pragma once

#include <arpa/inet.h>
#include <stdio.h>

#include <memory>
/* utils */
#include "crc32.h"
#include "crc32mpeg.h"
#include "robin_hood.h"
#include "utils_header.h"
#include "utils_log.h"
#include "utils_macro_multicore.h"

/*
 * Dyso Configuration
 * For simplicity, we assume Keyfield is uint32_t (4 Byte) and no data part
 * We use Robin-hood hash table.
 */
#define N_HEAD (10)  // number of heads (can be larger if you want)

/* macro functions */
#define OVERFLOW(X) (X >= 1.0)                                                 // true if value >= 1.0
#define UNDERFLOW(X) (X < 1.0)                                                 // true if value < 1.0
#define GET_HEAD_IDX(count) (std::min(31 - __builtin_clz(count), N_HEAD - 1))  // get head index from count

/**
 * 14-bit dyso index (crc32_mpeg)    | 26-bit hashkey (crc32)
 *             ||
 *             \/
 * 8-bit row index | 6-bit from reg  |
 */

// forward declaration
class Node;
class Head;

class Node {
   public:
    /* rule */
    uint32_t key_;  // Big-Endian (Network)

    /* cache status */
    double count_;   // relative count to its head's freq, [0, 1)
    uint8_t cache_;  // 1 if in cache, otherwise 0

    /* pointer to next/prev Node */
    struct Node* next_;
    struct Node* prev_;

    /* shared & weak pointer to Head */
    std::weak_ptr<Head> wHead_;

    Node(const uint32_t& key) : count_(0.0), cache_(0), next_(nullptr), prev_(nullptr) {
        key_ = key;
    }
    ~Node() {}
};

class Head {
   private:
    int idx_;                // index of head (-1 : backing (freq=0), >=0 : log_freq)
    double unitAdd_;         // unit to add count, e.g., 0.125 for frequency 8;
    struct Node* nodelist_;  // double-linked list

   public:
    Head(const int& idx, const double& unitAdd) : idx_(idx), unitAdd_(unitAdd), nodelist_(nullptr) {}
    ~Head() {}

    /* Accessor */
    const int& getIdx() const { return idx_; }
    const double& getUnitAdd() const { return unitAdd_; }
    Node* getNodeList() const { return nodelist_; }
    void setNodeList(Node* node) { nodelist_ = node; }

    /* Mutator */
    void aging() {
        --idx_;
        unitAdd_ = unitAdd_ * 2;
    }
    void pushNode(Node* node) {
        if (nodelist_ == nullptr) {
            // if empty list
            node->prev_ = node;
            node->next_ = nullptr;
            nodelist_ = node;
        } else {
            node->prev_ = nodelist_->prev_;
            node->next_ = nodelist_;
            nodelist_->prev_ = node;
            nodelist_ = node;
        }
    }

    // pop node
    void popNode(Node* node) {
        if (nodelist_ == node) {
            // pop from front
            nodelist_ = node->next_;
            if (nodelist_) {
                nodelist_->prev_ = node->prev_;
            }
        } else if (nodelist_->prev_ == node) {
            // pop from end
            node->prev_->next_ = node->next_;
            nodelist_->prev_ = node->prev_;
        } else {
            // pop at middle
            node->prev_->next_ = node->next_;
            if (node->next_ != nullptr)
                node->next_->prev_ = node->prev_;
        }
        /* Notice: The node's variables (prev/next/Head) will be set once allocated at its new Head. */
    }

    // pop and push front at the same Head
    void popThenPushNode(Node* node) {
        if (nodelist_ == node) {
            return;
        } else {
            // pop
            node->prev_->next_ = node->next_;
            if (nodelist_->prev_ == node)
                nodelist_->prev_ = node->prev_;
            else if (node->next_ != nullptr)
                node->next_->prev_ = node->prev_;

            // push
            node->prev_ = nodelist_->prev_;
            node->next_ = nodelist_;
            nodelist_->prev_ = node;
            nodelist_ = node;
        }
    }

    // Logging
    void printAll() {
        cPrint("Dyso-PrintAll", "-- Head Idx: %d", idx_);
        Node* node = nodelist_;
        while (node != nullptr) {
            std::shared_ptr<Head> sHead = node->wHead_.lock();
            if (sHead) {
                cPrint("Dyso-PrintAll", "Nodeinfo: [Head=%d] (%d) || Count: %.7f, Cache: %2d",
                       sHead->getIdx(),
                       ntohl(node->key_),
                       node->count_,
                       node->cache_);
            } else {
                cPrint("Dyso-PrintAll", "Nodeinfo: [Head=Expired] (%d) || Count: %.7f, Cache: %2d",
                       ntohl(node->key_),
                       node->count_,
                       node->cache_);
            }
            node = node->next_;
        }

        if (nodelist_ != nullptr) {
            node = nodelist_->prev_;
            cPrint("Dyso-PrintAll", "Last Node (nodelist->prev) : (%d) || Count: %.7f, Cache: %2d",
                   ntohl(node->key_),
                   node->count_,
                   node->cache_);
        }
    }
};

class Dyso {
   private:
    /* parameters */
    const uint32_t idx_;    // index of this Dyso object
    uint32_t agingPeriod_;  // aging period
    uint64_t totalCount_;   // total number of counts

    /* data */
    robin_hood::unordered_flat_map<uint32_t, Node*> hashmap_;  // crc32 26bit hashkey -> node
    std::vector<std::shared_ptr<Head>> heads_;
    std::vector<Node*> cchActive_;
    std::vector<Node*> cchUpdate_;
    bool replaceInProgress_;

    /* SPSC queue to DPDK TX Worker */
    qTxSPSC* updateQueue_ = nullptr;

    /* meta (only for replicas) */
    uint32_t virtHit_ = 0;   // virtual hit pkts
    uint32_t virtMiss_ = 0;  // virtual miss pkts

   public:
    Dyso(const uint32_t& idx,
         const uint32_t& agingPeriod)
        : idx_(idx), agingPeriod_(agingPeriod) {
        // load shared-memory queue
        updateQueue_ = getTxQueue(std::to_string(idx % NUM_DYSO_WORKER));

        // initialization
        totalCount_ = 0;
        replaceInProgress_ = false;
        // sanity check
        if (agingPeriod_ == 0) {
            std::invalid_argument("AgingPeriod should be larger than 0...");
            exit(1);
        }

        // initialize heads
        heads_.emplace_back(std::make_shared<Head>(Head(-1, 1.0)));
        for (size_t idx = 0; idx < N_HEAD; idx++) {
            heads_.emplace_back(std::move(std::make_shared<Head>(Head(idx, 1.0 / (1 << idx)))));
        }
        hashmap_.clear();
        cchActive_.resize(STAGE_CACHE);
        cchUpdate_.clear();
    }
    ~Dyso() {}

    void addDefaultNode(const uint32_t& key) {
        // input key is Big-Endian original key (Network-endian after htonl(.))
        // hashkey of 167772160 : 9309101 (26-bit)

        Node* node = new Node(key);
        heads_[0]->pushNode(node);  // add to head of idx=-1

        // add to hashmap
        uint8_t segkey[4];
        memcpy(segkey, (uint8_t*)(&key), 4);
        uint32_t hashkey = crc32_sw(segkey, 4) & REG_MASK_GET_HASHKEY;  // lower 26-bits hashkey
        hashmap_[hashkey] = node;
    }

    void removeNode(const uint32_t& key) {
        // input key is Big-Endian original key (network-endian after htonl(.))
        Node* node;
        try {
            uint8_t segkey[4];
            memcpy(segkey, (uint8_t*)(&key), 4);
            uint32_t hashkey = crc32_sw(segkey, 4) & REG_MASK_GET_HASHKEY;
            node = hashmap_.at(hashkey);
        } catch (const std::out_of_range& oor) {
            std::cerr << "[Dyso] Out of Range (removeNode): " << oor.what() << "\n";
            exit(1);
        }
        std::shared_ptr<Head> currPtr = node->wHead_.lock();
        if (currPtr) {
            currPtr->popNode(node);
        } else {
            heads_[0]->popNode(node);
        }
        delete node;
    }

    void doAging() {
        // (1) move nodes at idx=0 to idx=-1 (top: idx=0, bottom: idx=-1)
        Node* nodeToTop = heads_[1]->getNodeList();
        if (nodeToTop != nullptr) {
            Node* nodeToBottom = heads_[0]->getNodeList();
            Node* nodeToBottomTail = nodeToBottom->prev_;
            nodeToTop->prev_->next_ = nodeToBottom;
            nodeToBottom->prev_ = nodeToTop->prev_;
            nodeToTop->prev_ = nodeToBottomTail;
            heads_[0]->setNodeList(nodeToTop);
        }

        // (2) reset idx=0 then pop it
        heads_[1].reset();
        heads_.erase(std::next(heads_.begin()));

        // (3) aging headers
        for (std::vector<std::shared_ptr<Head>>::iterator it = std::next(heads_.begin()); it != heads_.end(); ++it)
            (*it)->aging();

        // (4) push back a new head
        heads_.emplace_back(std::make_shared<Head>(Head(N_HEAD - 1, 1.0 / (1 << (N_HEAD - 1)))));
    }

    /**
     * In fact, this "updating" step is not quite optimized, so can be enhanced with further efforts.
     */ 
    void updateNode(Node* node, const uint32_t& count) {
        // Count = 0 or Node* is not initialized, then skip
        if (!node || count == 0) {
            return;
        }

        // update total count
        totalCount_ = totalCount_ + count;

        // update and move if necessary
        std::shared_ptr<Head> currPtr = node->wHead_.lock();
        if (currPtr) {
            // idx >= 0
            node->count_ = node->count_ + count * currPtr->getUnitAdd();
            if (UNDERFLOW(node->count_)) {
                currPtr->popThenPushNode(node);
            } else if (node->wHead_.lock()->getIdx() == N_HEAD - 1) {
                // last node with overflow -> just push to recency position
                currPtr->popThenPushNode(node);
            } else {
                // remove links at old head
                currPtr->popNode(node);
                /* update pointer / counter -> push to Head */
                uint32_t accumCount = (uint32_t)((1.0 + node->count_) * (1 << currPtr->getIdx()));
                int idxHeadToMove = GET_HEAD_IDX(accumCount);
                node->wHead_.reset();
                node->wHead_ = heads_[idxHeadToMove + 1];
                node->count_ = std::min((accumCount - (1 << idxHeadToMove)) * heads_[idxHeadToMove + 1]->getUnitAdd(), 1.0);
                heads_[idxHeadToMove + 1]->pushNode(node);
            }
        } else {
            // idx == -1, remove links at old head
            heads_[0]->popNode(node);
            /* update pointer / counter -> push to Head */
            if (count == 1) {
                /* simple case */
                node->wHead_ = heads_[1];
                node->count_ = 0.0;
                heads_[1]->pushNode(node);
            } else {
                /* jump case */
                int idxHeadToMove = GET_HEAD_IDX(count);
                node->wHead_ = heads_[idxHeadToMove + 1];
                node->count_ = (count - (1 << idxHeadToMove)) * heads_[idxHeadToMove + 1]->getUnitAdd();
                heads_[idxHeadToMove + 1]->pushNode(node);
            }
        }
    }

    /**
     * for main policy
     **/
    void updatePolicyStat(const uint32_t& hashKey, uint32_t count = 1) {
        assert(hashKey < (1 << REG_LEN_HASHKEY_BIT));  // 26-bit hashKey
        Node* node;

        // get node from hashmap
        try {
            node = hashmap_.at(hashKey);
        } catch (const std::out_of_range& oor) {
            std::cerr << "[UpdateStat] Out of Range (updateMissRepoNode): " << oor.what() << "\n";
            std::cerr << "Hashkey: " << hashKey << ", dysoIdx: " << this->idx_ << "\n";
            exit(1);
        }

        // try aging
        if (totalCount_ >= agingPeriod_) {
            doAging();        // aging
            totalCount_ = 0;  // initialize
        }

        // update node
        updateNode(node, count);

        // try to make decision, if small buffer and missed
        if (!node->cache_ && replaceInProgress_ == false && updateQueue_->check_full() == false) {
            makeUpdateRequest();
        }
    }

    void makeUpdateRequest() {
        Node* node;
        uint32_t out_of_order = 0;
        std::vector<Node*> topK;
        std::vector<std::shared_ptr<Head>>::reverse_iterator rit;
        for (rit = heads_.rbegin(); rit != heads_.rend(); ++rit) {
            node = (*rit)->getNodeList();
            if (node == nullptr)
                continue;

            while (node != nullptr) {
                out_of_order = out_of_order + (1 - node->cache_);
                topK.push_back(node);
                if (topK.size() == STAGE_CACHE)
                    goto decision;
                node = node->next_;
            }
        }
        assert(false);

    decision:
        if (out_of_order > 0) {
            /* push a request to the DPDK-Tx Queue
             craft Update Packet header (keys are Bit-Endian (network)) */
            pcpp::dysoCtrlhdr* fetched = nullptr;

            /* insert to updateQueue_ */
            if ((fetched = updateQueue_->alloc()) != nullptr) {
                fetched->index_update = htonl(this->idx_);
                fetched->key0 = topK[0]->key_;
                fetched->key1 = topK[1]->key_;
                fetched->key2 = topK[2]->key_;
                fetched->key3 = topK[3]->key_;
                updateQueue_->push();
            } else {
                // queue is full, so skip updating
                // return;

                std::cerr << "[ERROR] DySO's TxQueue violates SPSC Queue!!" << std::endl;
                exit(1);
            }

            /* change to status -> on-going state update */
            cchUpdate_ = topK;
            replaceInProgress_ = true;
        }
    }

    void moveUpdateToActive() {
        /**
         * Just using "replaceInProgress_" may make this policy dead if no return of ACK (i.e., packet loss).
         * Especially, when UPDATE is dropped and never come back.
         * TODO: We may need to implement TIMEOUT module.
         */

        /* triggered once Dyso's CP gets Update Packet */
        if (cchUpdate_.empty() || replaceInProgress_ == false) {
            printf("[*WARNING*] Dyso %u (Progress:%d) ---> Mysterious ACK, maybe from prior experiment?\n",
                   this->idx_, replaceInProgress_);
            exit(1);
        }

        for (uint32_t i = 0; i < STAGE_CACHE; i++)
            if (cchActive_[i])
                cchActive_[i]->cache_ = 0;

        for (uint32_t i = 0; i < STAGE_CACHE; i++)
            if (cchUpdate_[i])
                cchUpdate_[i]->cache_ = 1;

        // move states: Update --> Active
        // cchUpdate_.resize(STAGE_CACHE);  // handle exception: # nodes < 4
        cchActive_ = cchUpdate_;
        cchUpdate_.clear();
        replaceInProgress_ = false;
    }

    /* for main policy */
    void adjustAgingPeriod(const uint32_t& newAgingPeriod) {
        agingPeriod_ = newAgingPeriod;
    }

    // API: printAll
    void printAll() {
        for (const auto& head : heads_) {
            head->printAll();
        }
        for (const auto& node : cchActive_) {
            if (node)
                cPrint("Dyso-PrintAll", "[CacheActive] Key: %d, Count: %f", ntohl(node->key_), node->count_);
        }
        for (const auto& node : cchUpdate_) {
            if (node)
                cPrint("Dyso-PrintAll", "[CacheUpdate] Key: %d, Count: %f", ntohl(node->key_), node->count_);
        }

        cPrint("Dyso-PrintAll", "============ Finished Prining =============\n");
    }

    /**
     * @brief Replica functions
     */

    void updatePolicyStatReplica(const uint32_t& hashKey, uint32_t& virtQLen, uint32_t count = 1) {
        assert(hashKey < (1 << REG_LEN_HASHKEY_BIT));  // 26-bit hashkey
        Node* node;
        // get node from hashmap
        try {
            node = hashmap_.at(hashKey);
        } catch (const std::out_of_range& oor) {
            std::cerr << "[UpdateStat-Replica] Out of Range (updateMissRepoNode): " << oor.what() << "\n";
            std::cerr << "Hashkey: " << hashKey << ", dysoIdx: " << idx_ << "\n";
            exit(1);
        }

        // update hit/miss rate
        auto hitOrMiss = node->cache_;
        (hitOrMiss == 1) ? ++(virtHit_) : ++(virtMiss_);

        // try aging
        if (totalCount_ >= agingPeriod_) {
            doAging();        // aging
            totalCount_ = 0;  // initialize
        }

        // update node
        updateNode(node, count);

        // try to make decision, if missed
        if (!hitOrMiss && virtQLen <= 1) {
            if (makeUpdateReuqestReplica()) {
                ++virtQLen;
            }
        }
    }

    /* for replica policy */
    bool makeUpdateReuqestReplica() {
        // pick top-(nCacheStage_) items
        Node* node;
        uint8_t out_of_order = 0;
        std::vector<Node*> topK;
        std::vector<std::shared_ptr<Head>>::reverse_iterator rit;
        for (rit = heads_.rbegin(); rit != heads_.rend(); ++rit) {
            node = (*rit)->getNodeList();
            if (node == nullptr)
                continue;

            while (node != nullptr) {
                out_of_order = out_of_order + (1 - node->cache_);
                topK.emplace_back(node);
                if (topK.size() == STAGE_CACHE)
                    goto decision;
                node = node->next_;
            }
        }

    decision:
        if (out_of_order > 0) {
            // update cache flags
            for (auto& node : cchActive_) {
                if (node)
                    node->cache_ = false;
            }
            for (auto& node : topK) {
                node->cache_ = true;
            }

            // copy topK to cchActive_
            cchActive_ = topK;
            return true;
        }
        return false;
    }

    /* for replica policy */
    void initAllReplica(const uint32_t& agingPeriod) {
        this->virtHit_ = 0;
        this->virtMiss_ = 0;

        // sanity check
        assert(this->cchUpdate_.empty());
        if (agingPeriod == 0) {
            std::invalid_argument("agingPeriod should be larger than 0...");
            exit(1);
        }

        // initialize agingPeriod_
        this->agingPeriod_ = agingPeriod;

        // clean cache information
        for (auto& node : this->cchActive_) {
            if (node) {
                node->cache_ = false;
            }
        }
        this->cchActive_.clear();
        this->cchActive_.resize(STAGE_CACHE);
        // clean node information (all nodes will be at headIdx=-1)
        for (size_t i = 0; i < N_HEAD; i++) {
            doAging();
        }
    }

    /* for replica policy */
    double getHitRate() const { return double(virtHit_) / (virtHit_ + virtMiss_); }
    double getMissRate() const { return double(virtMiss_) / (virtHit_ + virtMiss_); }
    void getAgingPeriod(uint32_t& agingPeriod) const { agingPeriod = this->agingPeriod_; }
    uint32_t getDysoIdx() const { return idx_; }
};
