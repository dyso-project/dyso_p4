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

// initialize a mbuf packet array of size 64
#define MAX_RECEIVE_BURST 64

class UpdateWorkerThread : public pcpp::DpdkWorkerThread {
   private:
    UpdateWorkerConfig& m_WorkerConfig;
    bool m_Stop;
    uint32_t m_CoreId;

   public:
    UpdateWorkerThread(UpdateWorkerConfig& workerConfig)
        : m_WorkerConfig(workerConfig),
          m_Stop(true),
          m_CoreId(MAX_NUM_OF_CORES + 1) {}
    ~UpdateWorkerThread() {}

    bool run(uint32_t coreId) {
        m_CoreId = coreId;
        m_Stop = false;

        uint32_t nRoundRobin = 0;  // to dequeue with round-robin
        qTxSPSC* m_updateQueue[NUM_DYSO_WORKER];
        for (uint32_t i = 0; i < NUM_DYSO_WORKER; i++) {
            m_updateQueue[i] = getTxQueue(std::to_string(i));  // get SPSC queues
            if (m_updateQueue[i] == nullptr) {
                std::cerr << "Failed to open qTxSPSC of idx -" << i << std::endl;
                exit(1);
            }
        }

        /* For DPDK */
        pcpp::MBufRawPacket* packetArr[MAX_RECEIVE_BURST] = {};
        uint32_t packetsReceived = 0;
        uint16_t ethertype = 0;

        /* For debugging */
        bool success_dequeue = false;
        uint64_t totalCount = 0;
        auto start = std::chrono::steady_clock::now();
        auto end = std::chrono::steady_clock::now();


        /* we use only one RxQueue / TxQueue per each core */
        auto rxQueueId = m_WorkerConfig.rxQueueList.front();
        auto txQueueId = m_WorkerConfig.txQueueList.front();
        assert(m_WorkerConfig.rxQueueList.size() == 1 && m_WorkerConfig.txQueueList.size() == 1);


        /* before starting simulation, refresh all results from prior experiments */
        pcpp::dysoCtrlhdr* dummyData;
        for (uint32_t i = 0; i < NUM_DYSO_WORKER; i++) {
            printf("[UpdateWorkerThread] Cleaning %u-th queues...\n", i);
            while ((dummyData = m_updateQueue[i]->front()) != nullptr)
                m_updateQueue[i]->pop();
            assert(m_updateQueue[i]->front() == nullptr);
        }
        
        printf("[UpdateWorkerThread] Successfully flushed all previous results.\nNow we can start new evaluation.\n");



        while (!m_Stop) {
            /* step 1. receive a batch of update packets from data plane */
            packetsReceived = m_WorkerConfig.recvPacketFrom->receivePackets(packetArr, MAX_RECEIVE_BURST, rxQueueId);

            for (uint32_t i = 0; i < packetsReceived; i++) {
                /* step 2. check the packet is UPDATE */
                pcpp::Packet parsedPacket(packetArr[i]);
                pcpp::EthLayer* ethernetLayer = parsedPacket.getLayerOfType<pcpp::EthLayer>();
                if (ethernetLayer == NULL) {
                    std::cerr << "[UpdateCore] Something went wrong, couldn't find Ethernet layer" << std::endl;
                    exit(1);
                }
                ethertype = pcpp::netToHost16(ethernetLayer->getEthHeader()->etherType);

                /* Ether type (update: 0xDEAD=57005) */
                /* Update Header then send back to Data plane */
                if (ethertype == 57005) {
                    /* UPDATE */
                    pcpp::Layer* dysoLayer = ethernetLayer->getNextLayer();
                    if (dysoLayer == NULL) {
                        std::cerr << "[UpdateCore] Something went wrong, couldn't find DysoUpdate layer" << std::endl;
                        exit(1);
                    }
                    pcpp::dysoCtrlhdr* data = (pcpp::dysoCtrlhdr*)dysoLayer->getData();
                    /***********/

                    /* dequeue in a round-robin and send packet with data */
                    pcpp::dysoCtrlhdr* fetched;
                    success_dequeue = false;
                    for (uint32_t i = nRoundRobin; i < nRoundRobin + NUM_DYSO_WORKER; i++) {
                        if ((fetched = m_updateQueue[i % NUM_DYSO_WORKER]->front()) != nullptr) {
                            memcpy(data, fetched, sizeof(pcpp::dysoCtrlhdr));
                            m_updateQueue[i % NUM_DYSO_WORKER]->pop();
#if (DYSODEBUG == 2)
                            printf("[UpdateWorkerThread] Sent update of %u-th bin\n", ntohl(data->index_update));
#endif  
                            m_WorkerConfig.sendPacketTo->sendPacket(parsedPacket, txQueueId, false);
                            success_dequeue = true;
                            nRoundRobin = (i + 1) % NUM_DYSO_WORKER;
                            break;
                        }
                    }

                    // if failed to dequeue (i.e., nothing to update)
                    if (!success_dequeue) {
                        data->index_update = htonl(7777777);
                        m_WorkerConfig.sendPacketTo->sendPacket(parsedPacket, txQueueId, false);
                    }

                    totalCount++;
                }
            }

#if (DYSODEBUG == 1)
            if (totalCount > (1 << 20)) {
                end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                printf("[UpdateWorkerThread] Time to process 1 packet: %lu (ns) \n", uint64_t(elapsed) / totalCount);
                totalCount = 0;
                start = end;
            }
#endif

        }
        return true;
    }

    void stop() {
        m_Stop = true;
    }

    uint32_t getCoreId() const {
        return m_CoreId;
    }
};