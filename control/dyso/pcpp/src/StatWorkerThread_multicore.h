#pragma once

#include <arpa/inet.h>

// Custom headers
#include "utils_header.h"
#include "utils_macro_multicore.h"
#include "utils_pcpp.h"

// DPDK headers
#include "DpdkDevice.h"
#include "DpdkDeviceList.h"
#include "EthLayer.h"
#include "IPv4Layer.h"
#include "Packet.h"

// initialize a mbuf packet array of size 64 of DPDK
#define MAX_RECEIVE_BURST 64

class StatWorkerThread : public pcpp::DpdkWorkerThread {
   private:
    StatWorkerConfig& m_WorkerConfig;
    bool m_Stop;
    uint32_t m_CoreId;

   public:
    StatWorkerThread(StatWorkerConfig& workerConfig)
        : m_WorkerConfig(workerConfig),
          m_Stop(true),
          m_CoreId(MAX_NUM_OF_CORES + 1) {}
    ~StatWorkerThread() {}

    bool run(uint32_t cordId) {
        m_CoreId = cordId;
        m_Stop = false;

        /* SPSC shared-memory queue for each DYSO_WORKER */
        qRxSPSC* m_statQueue[NUM_DYSO_WORKER];
        for (uint32_t i = 0; i < NUM_DYSO_WORKER; i++) {
            m_statQueue[i] = getRxQueue(std::to_string(i));  // get SPSC queues
            if (m_statQueue[i] == nullptr) {
                std::cerr << "Failed to open qRxSPSC of idx -" << i << std::endl;
                exit(1);
            }
        }
        std::queue<uint64_t> rxBulkMsgQueue[NUM_DYSO_WORKER];  // bulkMsgQueue for each SPSC queue
        std::queue<uint64_t> ackQueue[NUM_DYSO_WORKER];        // ACK queue for lossless monitoring
        uint32_t nRoundRobin = 0;                              // to dequeue with round-robin

        /* For DPDK */
        pcpp::MBufRawPacket* packetArr[MAX_RECEIVE_BURST] = {};  // DPDK RX packet array
        uint32_t packetsReceived = 0;
        uint16_t ethertype = 0;
        uint64_t* dummyMsg = nullptr;

        /* we use only one RxQueue DPDK per each core */
        auto rxQueueId = m_WorkerConfig.rxQueueList.front();
        assert(m_WorkerConfig.rxQueueList.size() == 1);

        /* before starting simulation, refresh all results from prior experiments */
        for (uint32_t i = 0; i < NUM_DYSO_WORKER; i++) {
            printf("[StatWorkerThread] Cleaning %u-th queues...\n", i);
            while ((dummyMsg = m_statQueue[i]->front()) != nullptr)
                m_statQueue[i]->pop();
            assert(m_statQueue[i]->front() == nullptr);
        }

        printf("[StatWorkerThread] Successfully flushed all previous results.\nNow we can start new evaluation.\n");

        /* For debugging */
        uint64_t totalCount = 0;
        auto start = std::chrono::steady_clock::now();
        auto end = std::chrono::steady_clock::now();

        while (!m_Stop) {
            // receive a batch of packets
            packetsReceived = m_WorkerConfig.recvPacketFrom->receivePackets(packetArr, MAX_RECEIVE_BURST, rxQueueId);

            /* iterate for each received pkt */
            for (uint32_t i = 0; i < packetsReceived; i++) {
                pcpp::Packet parsedPacket(packetArr[i]);
                pcpp::EthLayer* ethernetLayer = parsedPacket.getLayerOfType<pcpp::EthLayer>();
                if (ethernetLayer == NULL) {
                    std::cerr << "[StatWorkerThread] Couldn't find Ethernet layer" << std::endl;
                    return 1;
                }

                ethertype = pcpp::netToHost16(ethernetLayer->getEthHeader()->etherType);

                /* Ether type (control: 0xDEAD=57005) */
                if (ethertype == 57005) {
                    pcpp::Layer* dysoUpdateLayer = ethernetLayer->getNextLayer();
                    if (dysoUpdateLayer == NULL) {
                        std::cerr << "[StatWorkerThread] Couldn't find DysoUpdate layer" << std::endl;
                        return 1;
                    }
                    pcpp::dysoCtrlhdr* data = (pcpp::dysoCtrlhdr*)dysoUpdateLayer->getData();
                    enQueueCtrlPkt(data, rxBulkMsgQueue);
                }
            }

            /* Flush previously failed ACK msgs */
            nRoundRobin = (nRoundRobin + 1) % NUM_DYSO_WORKER;
            while (!ackQueue[nRoundRobin].empty()) {
                if ((dummyMsg = m_statQueue[nRoundRobin]->alloc()) != nullptr) {
                    *dummyMsg = ackQueue[nRoundRobin].front();
                    m_statQueue[nRoundRobin]->push();
                    ackQueue[nRoundRobin].pop();
                } else {
                    break;
                }
            }

            /* Flush to shared memory queues */
            for (uint32_t i = 0; i < NUM_DYSO_WORKER; i++) {
                for (; !rxBulkMsgQueue[i].empty(); rxBulkMsgQueue[i].pop()) {
                    uint64_t& msg = rxBulkMsgQueue[i].front();
                    if ((dummyMsg = m_statQueue[i]->alloc()) != nullptr) {
                        *dummyMsg = msg;
                        m_statQueue[i]->push();
                        totalCount += 1;
                    } else {
                        // std::cerr << "[StatWorkerThread] queue overflow at dyso_worker" << i << std::endl;
                        // exit(0);

                        /* Keep the ACK packets and retry later */
                        if ((msg & MSG_MASK_UPDATE_FLAG) == MSG_MASK_UPDATE_FLAG) {
                            ackQueue[i].push(msg);
#if (DYSODEBUG == 2)
                            printf("[StatWorkerThread] failed ack digest, core: %u, idx: %lu, totalCount: %lu\n",
                                   i, (msg - MSG_MASK_UPDATE_FLAG) >> 32, totalCount);
#endif
                        }
#if (DYSODEBUG == 2)
                        else {
                            printf("[StatWorkerThread] failed msg digest\n");
                        }
#endif
                    }
                }
            }

#if (DYSODEBUG >= 1)
            if (totalCount > (1 << 20)) {
                end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                printf("[StatWorkerThread] Time to process 1 msg: %lu (ns)\n", uint64_t(elapsed) / totalCount);
                printf("[StatWorkerThread] ACKBufferSize: %lu, %lu, %lu, %lu\n",
                       ackQueue[0].size(), ackQueue[1].size(), ackQueue[2].size(), ackQueue[3].size());
                totalCount = 0;
                start = end;
            }
#endif
        }
        return true;
    }

    void enQueueCtrlPkt(pcpp::dysoCtrlhdr* data, std::queue<uint64_t>* bulkMsgQueue) {
        // index of update
        uint32_t index_update = (ntohl(data->index_update));  // index of updated dyso
        if (index_update != 7777777) {                        // ignore empty-update
            uint64_t msg_update = (uint64_t(index_update) << 32) + MSG_MASK_UPDATE_FLAG;
            bulkMsgQueue[index_update % NUM_DYSO_WORKER].push(msg_update);
        }

        // index of probe
        uint32_t index_probe = (ntohl(data->index_probe) << REG_LEN_DYSO_IDX_BIT);  // crc32_mpeg[13:6]

        // rec0
        uint32_t reg0 = ntohl(data->rec0);
        if (reg0 != 0) {
            uint64_t msg0 = createMsgToStatThread(index_probe + (reg0 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg0);
            bulkMsgQueue[(reg0 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg0);
        }

        // rec1
        uint32_t reg1 = ntohl(data->rec1);
        if (reg1 != 0) {
            uint64_t msg1 = createMsgToStatThread(index_probe + (reg1 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg1);
            bulkMsgQueue[(reg1 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg1);
        }

        // rec2
        uint32_t reg2 = ntohl(data->rec2);
        if (reg2 != 0) {
            uint64_t msg2 = createMsgToStatThread(index_probe + (reg2 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg2);
            bulkMsgQueue[(reg2 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg2);
        }

        // rec3
        uint32_t reg3 = ntohl(data->rec3);
        if (reg3 != 0) {
            uint64_t msg3 = createMsgToStatThread(index_probe + (reg3 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg3);
            bulkMsgQueue[(reg3 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg3);
        }

        // rec4
        uint32_t reg4 = ntohl(data->rec4);
        if (reg4 != 0) {
            uint64_t msg4 = createMsgToStatThread(index_probe + (reg4 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg4);
            bulkMsgQueue[(reg4 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg4);
        }

        // rec5
        uint32_t reg5 = ntohl(data->rec5);
        if (reg5 != 0) {
            uint64_t msg5 = createMsgToStatThread(index_probe + (reg5 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg5);
            bulkMsgQueue[(reg5 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg5);
        }

        // rec6
        uint32_t reg6 = ntohl(data->rec6);
        if (reg6 != 0) {
            uint64_t msg6 = createMsgToStatThread(index_probe + (reg6 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg6);
            bulkMsgQueue[(reg6 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg6);
        }

        // rec3
        uint32_t reg7 = ntohl(data->rec7);
        if (reg7 != 0) {
            uint64_t msg7 = createMsgToStatThread(index_probe + (reg7 >> REG_LEN_HASHKEY_BIT), REG_MASK_GET_HASHKEY & reg7);
            bulkMsgQueue[(reg7 >> REG_LEN_HASHKEY_BIT) % NUM_DYSO_WORKER].push(msg7);
        }
    }

    void stop() {
        m_Stop = true;
    }

    uint32_t getCoreId() const {
        return m_CoreId;
    }
};
