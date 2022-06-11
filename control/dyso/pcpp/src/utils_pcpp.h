#pragma once

#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <queue>

#include "DpdkDevice.h"
#include "DpdkDeviceList.h"
#include "PcapPlusPlusVersion.h"
#include "SystemUtils.h"
#include "TablePrinter.h"

#define MBUF_POOL_SIZE (16 * 1024 - 1)
#define DEVICE_ID_UPDATE (0)
#define DEVICE_ID_STAT (1)

void onApplicationInterrupted(void* cookie) {
    std::cout << std::endl
              << "Shutting down all worker threads..." << std::endl;
    // stop worker threads
    pcpp::DpdkDeviceList::getInstance().stopDpdkWorkerThreads();
    exit(0);
}

/**
 * Macros for exiting the application with error
 */
#define EXIT_WITH_ERROR(reason)                       \
    do {                                              \
        std::cout << std::endl                        \
                  << "ERROR: " << reason << std::endl \
                  << std::endl;                       \
        exit(1);                                      \
    } while (0)

/**
 * Print application version
 */
void printAppVersion() {
    std::cout << "*** App Version:" << std::endl;
    std::cout
        << pcpp::AppName::get() << " " << pcpp::getPcapPlusPlusVersionFull() << std::endl
        << "Built: " << pcpp::getBuildDateTime() << std::endl
        << "Built from: " << pcpp::getGitInfo() << std::endl;
    std::cout << std::endl;
}

/**
 * Print to console all available DPDK ports. Used by the -l switch
 */
void listDpdkPorts() {
    std::cout << "*** DPDK port list:" << std::endl;
    // go over all available DPDK devices and print info for each one
    std::vector<pcpp::DpdkDevice*> deviceList = pcpp::DpdkDeviceList::getInstance().getDpdkDeviceList();
    for (std::vector<pcpp::DpdkDevice*>::iterator iter = deviceList.begin(); iter != deviceList.end(); iter++) {
        pcpp::DpdkDevice* dev = *iter;
        std::cout << "   "
                  << " Port #" << dev->getDeviceId() << ":"
                  << " MAC address='" << dev->getMacAddress() << "';"
                  << " PCI address='" << dev->getPciAddress() << "';"
                  << " PMD='" << dev->getPMDName() << "'"
                  << std::endl;
    }
    std::cout << std::endl;
}

/**
 * Print helper functions
 */
void printStats(pcpp::DpdkDevice* rxDevice) {
    pcpp::DpdkDevice::DpdkDeviceStats rxStats;
    rxDevice->getStatistics(rxStats);

    std::vector<std::string> columnNames;
    columnNames.push_back(" ");
    columnNames.push_back("Total Packets");
    columnNames.push_back("Packets/sec");
    columnNames.push_back("Bytes");
    columnNames.push_back("Bits/sec");

    std::vector<int> columnLengths;
    columnLengths.push_back(10);
    columnLengths.push_back(15);
    columnLengths.push_back(15);
    columnLengths.push_back(15);
    columnLengths.push_back(15);

    pcpp::TablePrinter printer(columnNames, columnLengths);

    std::stringstream totalRx;
    totalRx << "rx"
            << "|" << rxStats.aggregatedRxStats.packets << "|" << rxStats.aggregatedRxStats.packetsPerSec << "|" << rxStats.aggregatedRxStats.bytes << "|" << rxStats.aggregatedRxStats.bytesPerSec * 8;
    printer.printRow(totalRx.str(), '|');
}

/**
 * Contains all the configuration needed for the StatWorkerThread including:
 * - Which DPDK ports and which RX queues to receive packet from
 */
struct StatWorkerConfig {
    uint32_t CoreId;
    std::vector<int> rxQueueList;
    pcpp::DpdkDevice* recvPacketFrom;
    StatWorkerConfig() : CoreId(MAX_NUM_OF_CORES + 1), recvPacketFrom(NULL) {}
};

/**
 * Contains all the configuration needed for the UpdateWorkerThread including:
 * - Which DPDK ports and which RX queues to receive packet from
 * - Which DPDK ports to send packet to
 */
struct UpdateWorkerConfig {
    uint32_t CoreId;
    std::vector<int> rxQueueList;
    std::vector<int> txQueueList;
    pcpp::DpdkDevice* recvPacketFrom;
    pcpp::DpdkDevice* sendPacketTo;
    UpdateWorkerConfig() : CoreId(MAX_NUM_OF_CORES + 1), recvPacketFrom(NULL), sendPacketTo(NULL) {}
};

/**
 *  One StatWorkerThread will have one dedicated CPU core
 */
void prepareCoreConfiguration(pcpp::DpdkDevice* dpdkDeviceStat,
                              pcpp::DpdkDevice* dpdkDeviceUpdate,
                              std::vector<pcpp::SystemCore>& coresAvailable,
                            //   StatWorkerConfig statWorkerConfigArr[],
                            StatWorkerConfig* statWorkerConfigArr,
                              const uint32_t& nTotalStatRxQueue,
                              const uint32_t& nCoreForStat,
                            //   UpdateWorkerConfig updateWorkerConfigArr[],
                              UpdateWorkerConfig* updateWorkerConfigArr,
                              const uint32_t& nTotalUpdateRxQueue,
                              const uint32_t& nTotalUpdateTxQueue,
                              const uint32_t& nCoreForUpdate) {
    // sanity check
    if (coresAvailable.size() < nCoreForStat + nCoreForUpdate) {
        EXIT_WITH_ERROR("Not enough cores available. Try to reduce cores...");
    }

    /**
     * For UpdateWorkerThread
     */
    // core iterator to allocate
    auto coreIter = coresAvailable.begin();  

    // Configuration of Update Cores
    uint32_t totalUpdateNumOfRxQueues = nTotalUpdateRxQueue;
    uint32_t totalUpdateNumOfTxQueues = nTotalUpdateTxQueue;
    uint32_t numOfRxQueuePerUpdateCore = totalUpdateNumOfRxQueues / nCoreForUpdate;
    uint32_t residualRxQueueUpdateCore = totalUpdateNumOfRxQueues % nCoreForUpdate;
    uint32_t numOfTxQueuePerUpdateCore = totalUpdateNumOfTxQueues / nCoreForUpdate;
    uint32_t residualTxQueueUpdateCore = totalUpdateNumOfTxQueues % nCoreForUpdate;

    uint32_t updateIter = 0;        // worker index
    uint32_t idxUpdateRxQueue = 0;  // rxQueue index for allocation
    uint32_t idxUpdateTxQueue = 0;  // txQueue index for allocation
    
    for (; coreIter != coresAvailable.end(); coreIter++) {
        // assign core
        updateWorkerConfigArr[updateIter].CoreId = coreIter->Id;
        updateWorkerConfigArr[updateIter].recvPacketFrom = dpdkDeviceUpdate;
        updateWorkerConfigArr[updateIter].sendPacketTo = dpdkDeviceUpdate;

        // assign RxQueue per core
        for (uint32_t j = 0; j < numOfRxQueuePerUpdateCore; j++) {
            updateWorkerConfigArr[updateIter].rxQueueList.push_back(idxUpdateRxQueue);
            ++idxUpdateRxQueue;
        }
        // assign residual RxQueue to every cores at once
        if (coreIter != coresAvailable.end() && residualRxQueueUpdateCore > 0) {
            updateWorkerConfigArr[updateIter].rxQueueList.push_back(idxUpdateRxQueue);
            ++idxUpdateRxQueue;
            --residualRxQueueUpdateCore;
        }

        // assign TxQueue per core
        for (uint32_t j = 0; j < numOfTxQueuePerUpdateCore; j++) {
            updateWorkerConfigArr[updateIter].txQueueList.push_back(idxUpdateTxQueue);
            ++idxUpdateTxQueue;
        }
        // assign residual TxQueue to every cores at once
        if (coreIter != coresAvailable.end() && residualTxQueueUpdateCore > 0) {
            updateWorkerConfigArr[updateIter].txQueueList.push_back(idxUpdateTxQueue);
            ++idxUpdateTxQueue;
            --residualTxQueueUpdateCore;
        }

        updateIter++;
        // stop if all cores are allocated
        if (updateIter == nCoreForUpdate) {
            coreIter++; // move to next core and quit
            break;
        }

        // sanity check
        if (idxUpdateRxQueue >= totalUpdateNumOfRxQueues || idxUpdateTxQueue >= totalUpdateNumOfTxQueues) {
            EXIT_WITH_ERROR("RxQueue allocation is wrong.");
        }
    }

    /**
     * For StatWorkerThread
     */
    // Configuration of Stat Cores
    uint32_t totalStatNumOfRxQueues = nTotalStatRxQueue;
    uint32_t numOfRxQueuePerStatCore = totalStatNumOfRxQueues / nCoreForStat;
    uint32_t residualRxQueueStatCore = totalStatNumOfRxQueues % nCoreForStat;

    uint32_t statIter = 0;                   // worker index
    uint32_t idxStatRxQueue = 0;             // rxQueue index for allocation

    for (; coreIter != coresAvailable.end(); coreIter++) {
        // assign core
        statWorkerConfigArr[statIter].CoreId = coreIter->Id;
        statWorkerConfigArr[statIter].recvPacketFrom = dpdkDeviceStat;

        // assign RxQueue per core
        for (uint32_t j = 0; j < numOfRxQueuePerStatCore; j++) {
            statWorkerConfigArr[statIter].rxQueueList.push_back(idxStatRxQueue);
            ++idxStatRxQueue;
        }

        // assign residual RxQueue to every cores at once
        if (coreIter != coresAvailable.end() && residualRxQueueStatCore > 0) {
            statWorkerConfigArr[statIter].rxQueueList.push_back(idxStatRxQueue);
            ++idxStatRxQueue;
            --residualRxQueueStatCore;
        }

        statIter++;
        // stop if all cores are allocated
        if (statIter == nCoreForStat) {
            break;
        }

        // sanity check
        if (idxStatRxQueue >= totalStatNumOfRxQueues) {
            EXIT_WITH_ERROR("RxQueue allocation is wrong.");
        }
    }

    /**
     * Print configuration per core
     */

    // Update Core
    std::cout << "\n[Config] DPDK UpdateDevice#" << dpdkDeviceUpdate->getDeviceId() << std::endl;
    for (uint32_t i = 0; i < nCoreForUpdate; i++) {
        // print coreId
        std::cout << "    CoreID: " << updateWorkerConfigArr[i].CoreId << " -> ";

        // print RxQueue Ids
        std::cout << "RxQueue: ";
        for (auto iter = updateWorkerConfigArr[i].rxQueueList.begin(); iter != updateWorkerConfigArr[i].rxQueueList.end(); iter++) {
            std::cout << *iter << ",";
        }

        // print TxQueue Ids
        std::cout << "    TxQueue: ";
        for (auto iter = updateWorkerConfigArr[i].txQueueList.begin(); iter != updateWorkerConfigArr[i].txQueueList.end(); iter++) {
            std::cout << *iter << ",";
        }
        std::cout << std::endl;
    }

    // Stat Core
    std::cout << "\n[Config] DPDK StatDevice#" << dpdkDeviceStat->getDeviceId() << std::endl;
    for (uint32_t i = 0; i < nCoreForStat; i++) {
        // print coreId
        std::cout << "    CoreID: " << statWorkerConfigArr[i].CoreId << " -> ";

        // print RxQueue Ids
        std::cout << "RxQueue: ";
        for (auto iter = statWorkerConfigArr[i].rxQueueList.begin(); iter != statWorkerConfigArr[i].rxQueueList.end(); iter++) {
            std::cout << *iter << ",";
        }
        std::cout << std::endl;
    }

}
