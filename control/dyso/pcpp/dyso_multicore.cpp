#include "src/dyso_multicore.hpp"


/**
 * 
 * (1) Processing
 * On Tofino's main CPU cores, each thread can handle ~2M messages per second.
 * In this prototype, we can cover 1M control packets (i.e. 9M messages) per second.
 * 
 * (2) Messages from DPDK's RX, and to DPDK's TX
 * 
 * We leverage the shared memory queue (SPSC) whose source is in "src/utils_macro_multicore.h"
 * 
 * (3) Hard-coded numbers
 *  -- Refer to "src/utils_macro_multicore.h"
 * 
 * Total entries : 64K, which consists of k = 4 chaining and 16K rows in the data plane
 * Each row's items are managed "disjointly" by independent "dyso" policies.
 * For simpliciy, we assume to use 4Byte items.
 * 
 * (4) Source code of data structure
 *  -- Refer to "src/dyso.hpp"
 * 
 */

int main(int argc, char const* argv[]) {
    if (argc != 2) {
        std::cerr << "Must put ONE argument for queue index, e.g., one of {0, 1, 2, 3}." << std::endl;
        exit(1);
    }

    /* get RX Msg Queue (via shared memory) from DPDK's StatThread */
    uint32_t dyso_index_ = atoi(argv[1]);
    assert(dyso_index_ < NUM_DYSO_WORKER);  // sanity check
    printf("Running DySO of Core %u\n", dyso_index_);
    qRxSPSC* rxQueue = getRxQueue(std::string(argv[1]).c_str());

    /* initialize DySO's default nodes (for read-centric evaluation) */
    uint32_t agingPeriod = 16;  // global aging period (to be adjusted)
    std::vector<Dyso> dyso;
    std::vector<Dyso> dysoReplicaUp;
    std::vector<Dyso> dysoReplicaDown;

    // REG_LEN_KEY : number of rows (or dyso policies)
    for (uint32_t idx = 0; idx < REG_LEN_KEY; idx++) {
        dyso.emplace_back(Dyso(idx, agingPeriod));
        // create replicas
        if (checkReplica(idx, dyso_index_)) {
            auto replicaDysoIdx = getReplicaDysoIdx(idx);
            dysoReplicaUp.emplace_back(Dyso(replicaDysoIdx, agingPeriod * 2));
            dysoReplicaDown.emplace_back(Dyso(replicaDysoIdx, std::max(agingPeriod / 2, uint32_t(1))));
        }
    }

    printf("--------\n[%u] Generated %lu dyso, and (%lu)x2 up/down replicas\n",
           dyso_index_, dyso.size(), dysoReplicaUp.size());

    /* pre-install the nodes of 4B keys to be queried in the simulation */
    const uint64_t upperSrcIP = (uint64_t(5) << 20);     // [0, 5M)
    const uint64_t lowerSrcIP = (uint64_t(4085) << 20);  // [4085M, 4096M)
    printf("[%u] Start initializing Dyso nodes...\n", dyso_index_);
    printf("[%u] agingPeriod: %u\n", dyso_index_, agingPeriod);
    printf("[%u] Range: [%u, %lu] and [%lu, %u]\n", dyso_index_, uint32_t(0), upperSrcIP, lowerSrcIP, UINT32_MAX);

    // generate candidate nodes
    uint8_t tempIP[4];
    for (uint64_t srcIP = 0; srcIP < upperSrcIP; srcIP++) {
        uint32_t netSrcIP = htonl(uint32_t(srcIP));          // change byte orders
        memcpy(tempIP, (uint8_t*)(&netSrcIP), 4);            // srcIP
        uint32_t idx = crc32_mpeg(tempIP, 4) % REG_LEN_KEY;  // get dyso's index

        // check the flow is associated to this core
        if (getReplicaThreadIdx(idx) == dyso_index_) {
            dyso[idx].addDefaultNode(netSrcIP);  // insert

            // insert to replicas
            if (checkReplica(idx, dyso_index_)) {
                uint32_t replicaDysoIdx = getReplicaDysoIdx(idx);
                dysoReplicaUp[replicaDysoIdx].addDefaultNode(netSrcIP);
                dysoReplicaDown[replicaDysoIdx].addDefaultNode(netSrcIP);
            }
        }
    }

    for (uint64_t srcIP = lowerSrcIP; srcIP <= UINT32_MAX; srcIP++) {
        uint32_t netSrcIP = htonl(uint32_t(srcIP));
        memcpy(tempIP, (uint8_t*)(&netSrcIP), 4);
        uint32_t idx = crc32_mpeg(tempIP, 4) % REG_LEN_KEY;

        // check the flow is associated to this core
        if (getReplicaThreadIdx(idx) == dyso_index_) {
            dyso[idx].addDefaultNode(netSrcIP);  // insert

            // insert to replicas
            if (checkReplica(idx, dyso_index_)) {
                uint32_t replicaDysoIdx = getReplicaDysoIdx(idx);
                dysoReplicaUp[replicaDysoIdx].addDefaultNode(netSrcIP);
                dysoReplicaDown[replicaDysoIdx].addDefaultNode(netSrcIP);
            }
        }
    }

    printf("[%u] Initializing Done.\n--------\n", dyso_index_);

    /* run by digesting the reports from data plane, and run self-tuning */
    uint64_t* fetched = nullptr;
    uint32_t hashkey, dysoIdx;
    uint64_t nCtrlPktRx = 0;
    uint32_t virtualQueueUp = 0;
    uint32_t virtualQueueDown = 0;
    std::queue<uint64_t> msgQueue;
    uint64_t clockCycle = 0;
    auto start = std::chrono::steady_clock::now();
    auto end = std::chrono::steady_clock::now();
    while (true) {
        // (1) flush the msgs from rxQueue (in batch of 1000)
        for (uint32_t i = 0; i < 1000; i++) {
            if ((fetched = rxQueue->front()) != nullptr) {
                msgQueue.push(*fetched);
                rxQueue->pop();
            } else {
                break;
            }
        }

        // (2) process in batch
        for (; !msgQueue.empty(); msgQueue.pop()) {
            uint64_t& msg = msgQueue.front();
            clockCycle++;

            // update msg
            if ((msg & MSG_MASK_UPDATE_FLAG) == MSG_MASK_UPDATE_FLAG) {
                dysoIdx = uint32_t((msg - MSG_MASK_UPDATE_FLAG) >> 32);
                nCtrlPktRx++;
                // need to manage the virtual queues for replicas by hand
                // the dequeu speed is 1/256 slower than main policy's TXqueue
                // because 4 DySO workers x 1/64 replica ratio
                if (nCtrlPktRx % REG_LEN_REC == 0) {  
                    virtualQueueUp = (virtualQueueUp == 0) ? 0 : virtualQueueUp - 1;
                    virtualQueueDown = (virtualQueueDown == 0) ? 0 : virtualQueueDown - 1;
                }
                dyso[dysoIdx].moveUpdateToActive();
            }
            // packet signatures (hash values for monitoring)
            else {
                // parse the message and feed to the corresponding policy (dysoIdx)
                parseMsgAtStatThread(msg, dysoIdx, hashkey);
                dyso[dysoIdx].updatePolicyStat(hashkey);

                // process for replicas
                if (checkReplica(dysoIdx, dyso_index_)) {
                    uint32_t replicaDysoIdx = getReplicaDysoIdx(dysoIdx);
                    dysoReplicaUp[replicaDysoIdx].updatePolicyStatReplica(hashkey, virtualQueueUp);
                    dysoReplicaDown[replicaDysoIdx].updatePolicyStatReplica(hashkey, virtualQueueDown);
                }

                // do aging (every 2M msgs ~ 1 second if control packet rate is 1Mpps)
                if (clockCycle > 2097152) {
                    // initialize a wall-clock
                    clockCycle = 0;
                    start = std::chrono::steady_clock::now();

                    // self-tuning aging period
                    double hitRatioUp = 0.0, hitRatioDown = 0.0;
                    for (auto& replica : dysoReplicaUp)
                        hitRatioUp += replica.getHitRate();
                    for (auto& replica : dysoReplicaDown)
                        hitRatioDown += replica.getHitRate();

                    // for sanity, we bound the aging period
                    agingPeriod = (hitRatioUp >= hitRatioDown) ? agingPeriod * 2 : std::max(agingPeriod / 2, uint32_t(1));
                    agingPeriod = (agingPeriod > 1024) ? 32 : agingPeriod;

                    // reconfigure all repllicas
                    for (auto& replica : dysoReplicaUp)
                        replica.initAllReplica(agingPeriod << 1);
                    for (auto& replica : dysoReplicaDown)
                        replica.initAllReplica(std::max(agingPeriod >> 1, uint32_t(1)));

                    // reconfigure main policies
                    for (auto& policy : dyso) {
                        if (getReplicaThreadIdx(policy.getDysoIdx()) == dyso_index_)
                            policy.adjustAgingPeriod(agingPeriod);
                    }
                }
            }
        }
    }

    return 0;
}
