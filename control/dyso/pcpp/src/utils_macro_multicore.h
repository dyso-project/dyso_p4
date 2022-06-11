#pragma once

#include <stdint.h>

#include <chrono>
#include <string>

/* for inter-process communications */
#include "SPSCQueue.h"
#include "shmmap.h"
#include "utils_header.h"

/* debugging flag */
#define DYSODEBUG (1) // 2: debugging, 1: warn, 0: info

/* Stage Allocation from P4*/
constexpr uint32_t STAGE_CACHE = 4;                                    // number of match-units
constexpr uint32_t STAGE_RECORD = 8;                                   // number of stages for packet fingerprints
constexpr uint32_t REG_LEN_KEY = (1 << 14);                            // number of rows in match-unit's hash table (RA size)
constexpr uint32_t REG_LEN_REC = (1 << 8);                             // number of rows for fingerprints' storage (RA size)
constexpr uint32_t REG_LEN_HASHKEY_BIT = 26;                           // In 32bit fingerprint, a hashkey is lower 26 bits
constexpr uint32_t REG_LEN_DYSO_IDX_BIT = (32 - REG_LEN_HASHKEY_BIT);  // In 32bit fingerprint, the upper 6 bits is a lower 6 bits of dyso index
constexpr uint32_t REG_MASK_GET_HASHKEY = 0x3FFFFFF;                   // lower 26 bits
constexpr uint32_t REG_MASK_GET_DYSO_IDX = 0x3FFF;                     // lower 14 bits of crc32_mpeg

/* Pcap++ & DPDK Engine Configuration */
constexpr uint32_t nCoreForStat = 1;                                                  // (DPDK) 1 core for 1 thread
constexpr uint32_t nCoreForUpdate = 1;                                                // (DPDK) 1 core for 1 thread
constexpr uint32_t maskCoreToUse = ((1 << (1 + nCoreForStat + nCoreForUpdate)) - 1);  // 7 = b'111, using cores 0,1,2
constexpr uint64_t MSG_MASK_UPDATE_FLAG = 0x8000000000000000;                         // (1000..)(00..00)
constexpr uint64_t MSG_MASK_GET_IDX = 0xFFFFFFFF00000000;                             // upper 32 bits
constexpr uint64_t MSG_MASK_GET_KEY = 0xFFFFFFFF;                                     // lower 32 bits

/* Number of DySo's multicore */
constexpr uint32_t NUM_DYSO_WORKER = 4;  // number of dyso's core

/**
 * Inline functions
 */

/* create and parse msg at stat threads */
inline uint64_t createMsgToStatThread(const uint32_t& dysoIdx, const uint32_t& hashkey) {
    uint64_t msg = (uint64_t(dysoIdx) << 32) + hashkey;
    return msg;
}
inline void parseMsgAtStatThread(const uint64_t& msg, uint32_t& dysoIdx, uint32_t& hashkey) {
    dysoIdx = uint32_t(msg >> 32);
    hashkey = uint32_t(msg & MSG_MASK_GET_KEY);
}

/* get dyso's index (lower 14 bits of crc32_mpeg) */
inline uint32_t getDysoIdx(const uint32_t& crc32_mpeg) {
    return crc32_mpeg & REG_MASK_GET_DYSO_IDX;  // return lower 14 bits
}

/* dyso's replica and thread index. dysoIdx must be 14bits */
inline uint32_t getReplicaDysoIdx(const uint32_t& dysoIdx) {
    return ((dysoIdx & 0x3F) >> 2);  // get [5:2] (4bits)
}
inline uint32_t getReplicaThreadIdx(const uint32_t& dysoIdx) {
    return (dysoIdx & 0x3);  // get [1:0] (2bits)
}
inline uint32_t checkReplica(const uint32_t& dysoIdx, const uint32_t& coreIdx) {
    return ((dysoIdx >> 6) == 0 && (dysoIdx & 0x3) == coreIdx) ? true : false;  // if [13:6] is 0
}


/**
 * SPSC queue implemented atop the shared memory
 * Note that they must be removed/cleaned in the beginning.
 * 
 * The description of connection:
 * 
 * ** DPDK RX Worker <------->  one SPSCRxQueue for each DySO Worker (total 4 expected) 
 * ** one SPSCTxQueue for each DySO worker (total 4 expected) <-------> DPDK TX Worker
 */

typedef SPSCQueue<uint64_t, 16384> qRxSPSC;
typedef SPSCQueue<pcpp::dysoCtrlhdr, 128> qTxSPSC;

qRxSPSC* getRxQueue(const std::string& name) {
    // std::cout << "Get SPSC RX queue with name: " << std::string("/shm_dyso_rx_queue_") + name << std::endl;
    return spsc_shmmap<qRxSPSC>((std::string("/shm_dyso_rx_queue_") + name).c_str());
}

qTxSPSC* getTxQueue(const std::string& name) {
    // std::cout << "Get SPSC TX queue with name: " << std::string("/shm_dyso_tx_queue_") + name << std::endl;
    return spsc_shmmap<qTxSPSC>((std::string("/shm_dyso_tx_queue_") + name).c_str());
}
