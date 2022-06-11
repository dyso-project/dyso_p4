#include "src/StatWorkerThread_multicore.h"
#include "src/UpdateWorkerThread_multicore.h"
#include "src/dyso_multicore.hpp"

int main(int argc, char* argv[]) {
    // print PCAP++ version
    printAppVersion();

    // Register the on-app-close event-handler
    pcpp::ApplicationEventHandler::getInstance().onApplicationInterrupted(onApplicationInterrupted, NULL);

    // Get and Set core masks to use
    pcpp::CoreMask coreMaskToUse = pcpp::getCoreMaskForAllMachineCores();
    std::cout << "All available core mask: " << coreMaskToUse << ", MAX_NUM_OF_CORES: " << MAX_NUM_OF_CORES << std::endl;
    coreMaskToUse = maskCoreToUse;  // e.g., 7 = b'111', use cores 0,1,2

    // extract core vector from core mask
    std::vector<pcpp::SystemCore> coresAvailable;
    pcpp::createCoreVectorFromCoreMask(coreMaskToUse, coresAvailable);

    // need minimum of 2 cores to start - 1 management core + 1 per-thread stat core  (+ 1 per-thread update core)
    if (coresAvailable.size() < 1 + nCoreForStat + nCoreForUpdate) {
        EXIT_WITH_ERROR("Needed minimum number of cores to start the application...");
    }

    // initialize DPDK and print available ports (DEVICE_ID) for DPDK
    if (!pcpp::DpdkDeviceList::initDpdk(coreMaskToUse, MBUF_POOL_SIZE)) {
        EXIT_WITH_ERROR("Couldn't initialize DPDK");
    } else {
        listDpdkPorts();
    }

    // removing DPDK master core from core mask because DPDK worker threads (e.g., stat and update) cannot run on master core
    coreMaskToUse = coreMaskToUse & ~(pcpp::DpdkDeviceList::getInstance().getDpdkMasterCore().Mask);

    // re-calculate cores to use after removing master core
    coresAvailable.clear();
    pcpp::createCoreVectorFromCoreMask(coreMaskToUse, coresAvailable);
    std::cout << "Core mask after removing master core: " << coreMaskToUse << std::endl;
    std::cout << "Available cores : ";
    for (auto iter = coresAvailable.begin(); iter != coresAvailable.end(); ++iter) {
        std::cout << uint32_t(iter->Id) << ", ";
    }
    std::cout << std::endl;

    // Find DPDK devices
    pcpp::DpdkDevice* dpdkDeviceUpdate = pcpp::DpdkDeviceList::getInstance().getDeviceByPort(DEVICE_ID_UPDATE);
    if (dpdkDeviceUpdate == NULL) {
        EXIT_WITH_ERROR("Cannot find dpdkDeviceUpdate with port '" << DEVICE_ID_UPDATE << "'");
    }
    pcpp::DpdkDevice* dpdkDeviceStat = pcpp::DpdkDeviceList::getInstance().getDeviceByPort(DEVICE_ID_STAT);
    if (dpdkDeviceStat == NULL) {
        EXIT_WITH_ERROR("Cannot find dpdkDeviceStat with port '" << DEVICE_ID_STAT << "'");
    }

    // open and allocate Rx Queues to dpdkDevice
    // uint32_t nTotalStatRxQueue = dpdkDeviceStat->getTotalNumOfRxQueues() >> 1; // 64 Rxqueues
    uint32_t nTotalStatRxQueue = nCoreForStat;  // total {nCoreForStat} RxQueue (1 queue - 1 core - 1 statWorker)
    std::cout << "nTotalStatRxQueue : " << nTotalStatRxQueue << std::endl;
    if (!dpdkDeviceStat->openMultiQueues(nTotalStatRxQueue, 0)) {
        EXIT_WITH_ERROR("Couldn't open DPDK StatDevice #" << dpdkDeviceStat->getDeviceId() << ", PMD '" << dpdkDeviceStat->getPMDName() << "'");
    }

    // total {nCoreForUpdate} Rx / Tx Queues (1 queue - 1 core - 1 updateWorker)
    uint32_t nTotalUpdateRxQueue = nCoreForUpdate;
    uint32_t nTotalUpdateTxQueue = nCoreForUpdate;
    std::cout << "nTotalUpdateRxQueue : " << nTotalUpdateRxQueue << std::endl;
    std::cout << "nTotalUpdateTxQueue : " << nTotalUpdateTxQueue << std::endl;
    if (!dpdkDeviceUpdate->openMultiQueues(nTotalUpdateRxQueue, nTotalUpdateTxQueue)) {
        EXIT_WITH_ERROR("Couldn't open DPDK UpdateDevice #" << dpdkDeviceUpdate->getDeviceId() << ", PMD '" << dpdkDeviceUpdate->getPMDName() << "'");
    }

    // prepare configuration for every core
    StatWorkerConfig statWorkerConfigArr[nCoreForStat];
    UpdateWorkerConfig updateWorkerConfigArr[nCoreForUpdate];

    prepareCoreConfiguration(dpdkDeviceStat,
                             dpdkDeviceUpdate,
                             coresAvailable,
                             statWorkerConfigArr,
                             nTotalStatRxQueue,
                             nCoreForStat,
                             updateWorkerConfigArr,
                             nTotalUpdateRxQueue,
                             nTotalUpdateTxQueue,
                             nCoreForUpdate);

    /********************* DPDK IS READY *******************/

    assert(nCoreForStat == 1);
    assert(nCoreForUpdate == 1);

    /**
     * Create and Start DPDK Worker Threads
     */
    
    std::vector<pcpp::DpdkWorkerThread*> dysoWorkerThreadVec;

    for (uint32_t i = 0; i < nCoreForStat; i++) {
        StatWorkerThread* newStatWorker = new StatWorkerThread(statWorkerConfigArr[i]);
        dysoWorkerThreadVec.push_back(newStatWorker);
    }
    for (uint32_t i = 0; i < nCoreForUpdate; i++) {
        UpdateWorkerThread* newUpdateWorker = new UpdateWorkerThread(updateWorkerConfigArr[i]);
        dysoWorkerThreadVec.push_back(newUpdateWorker);
    }

    // Start capture in async mode
    if (!pcpp::DpdkDeviceList::getInstance().startDpdkWorkerThreads(coreMaskToUse, dysoWorkerThreadVec)) {
        std::cerr << "Couldn't start worker threads" << std::endl;
        return 1;
    }

    printf("--------------\nStart running worker threads...\n");
    
    /* Run Indefinitely */
    printf("Run indefinitely...\n\n\n\n\n");
    while (true) {
        sleep(1);
    }

    pcpp::DpdkDeviceList::getInstance().stopDpdkWorkerThreads();
    return 0;
}
