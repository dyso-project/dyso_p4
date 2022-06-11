/* CREDITS:
This boiler plate code is heavily adapted from Intel Connectivity
Academy course ICA-1132: "Barefoot Runtime Interface & PTF"
*/

/* Standard Linux/C++ includes go here */
#include <bf_rt/bf_rt_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_session.hpp>
#include <bf_rt/bf_rt_table.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table_key.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
/* All fixed function API includes go here */
#include <bf_switchd/bf_switchd.h>

#ifdef __cplusplus
}
#endif

/*
 * Convenient defines that reflect SDE conventions
 */
#ifndef SDE_INSTALL
#error "Please add -DSDE_INSTALL=\"$SDE_INSTALL\" to CPPFLAGS"
#endif

#ifndef PROG_NAME
#error "Please add -DPROG_NAME=\"netcache\" to CPPFLAGS"
#endif

#define CONF_FILE_DIR "share/p4/targets/tofino"
#define CONF_FILE_PATH(prog) \
    SDE_INSTALL "/" CONF_FILE_DIR "/" prog ".conf"

#define INIT_STATUS_TCP_PORT 7777
#define BFSHELL SDE_INSTALL "/bin/bfshell"  // macro string concat

#define DEV_TGT_ALL_PIPES 0xFFFF
#define DEV_TGT_ALL_PARSERS 0xFF
#define CHECK_BF_STATUS(status) __check_bf_status__(status, __FILE__, __LINE__)
bf_status_t status;

void __check_bf_status__(bf_status_t status, const char *file, int lineNumber);
std::vector<uint64_t> parse_string_to_key(std::string str);

struct message {
    long msg_type;
    uint32_t srcIP;
};

static inline bool sortAscByVal(const std::pair<uint64_t, uint64_t> &a, const std::pair<uint64_t, uint64_t> &b) {
    return (a.second < b.second);
}

int app_dyso(bf_switchd_context_t *switchd_ctx) {
    (void)switchd_ctx;
    bf_rt_target_t dev_tgt;
    std::shared_ptr<bfrt::BfRtSession> session;
    const bfrt::BfRtInfo *bf_rt_info = nullptr;
    auto fromHwFlag = bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW;
    auto fromSwFlag = bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW;

    // to avoid unused compiler error
    printf("Flags - fromHwFlag=%ld, fromSwFlag=%ld\n",
           static_cast<uint64_t>(fromHwFlag), static_cast<uint64_t>(fromSwFlag));

    printf("Configuring via bfrt_python script...\n");
    fflush(stdout);
    std::string bfshell_pktgen = BFSHELL " -b " __DIR__ "/../debug/set_key_default.py";
    system(bfshell_pktgen.c_str());

    // printf("\n\n****** Press enter to run *****");
    // getchar();

    /* Prepare the dev_tgt */
    memset(&dev_tgt, 0, sizeof(dev_tgt));
    dev_tgt.dev_id = 0;
    dev_tgt.pipe_id = DEV_TGT_ALL_PIPES;

    /* Create BfRt session and retrieve BfRt Info */
    // Create a new BfRt session
    session = bfrt::BfRtSession::sessionCreate();
    if (session == nullptr) {
        printf("ERROR: Couldn't create BfRtSession\n");
        exit(1);
    }

    // Get ref to the singleton devMgr
    bfrt::BfRtDevMgr &dev_mgr = bfrt::BfRtDevMgr::getInstance();
    status = dev_mgr.bfRtInfoGet(dev_tgt.dev_id, PROG_NAME, &bf_rt_info);

    if (status != BF_SUCCESS) {
        printf("ERROR: Could not retrieve BfRtInfo: %s\n", bf_err_str(status));
        return status;
    }
    printf("Retrieved BfRtInfo successfully!\n");

    status = session->sessionCompleteOperations();
    CHECK_BF_STATUS(status);
    
    printf("Running DySO's control plane.....\n");

    // for measurement
    const bfrt::BfRtTable *reg_hit_number = nullptr;
    const bfrt::BfRtTable *reg_total_number = nullptr;

    bf_rt_id_t reg_hit_number_key_id, reg_hit_number_data_id;      // hit number's key, val
    bf_rt_id_t reg_total_number_key_id, reg_total_number_data_id;  // total number's key, val

    status = bf_rt_info->bfrtTableFromNameGet("Pipe1SwitchIngress.reg_hit_number", &reg_hit_number);
    CHECK_BF_STATUS(status);
    status = bf_rt_info->bfrtTableFromNameGet("Pipe1SwitchIngress.reg_total_number", &reg_total_number);
    CHECK_BF_STATUS(status);

    status = reg_hit_number->keyFieldIdGet("$REGISTER_INDEX", &reg_hit_number_key_id);
    CHECK_BF_STATUS(status);
    status = reg_hit_number->dataFieldIdGet("Pipe1SwitchIngress.reg_hit_number.f1", &reg_hit_number_data_id);
    CHECK_BF_STATUS(status);
    status = reg_total_number->keyFieldIdGet("$REGISTER_INDEX", &reg_total_number_key_id);
    CHECK_BF_STATUS(status);
    status = reg_total_number->dataFieldIdGet("Pipe1SwitchIngress.reg_total_number.f1", &reg_total_number_data_id);
    CHECK_BF_STATUS(status);

    std::unique_ptr<bfrt::BfRtTableKey> reg_hit_number_index, reg_total_number_index;
    std::unique_ptr<bfrt::BfRtTableData> reg_hit_number_value, reg_total_number_value;

    status = reg_hit_number->keyAllocate(&reg_hit_number_index);
    CHECK_BF_STATUS(status);
    status = reg_hit_number->dataAllocate(&reg_hit_number_value);
    CHECK_BF_STATUS(status);

    status = reg_total_number->keyAllocate(&reg_total_number_index);
    CHECK_BF_STATUS(status);
    status = reg_total_number->dataAllocate(&reg_total_number_value);
    CHECK_BF_STATUS(status);

    status = reg_hit_number->keyReset(reg_hit_number_index.get());
    CHECK_BF_STATUS(status);
    status = reg_hit_number->dataReset(reg_hit_number_value.get());
    CHECK_BF_STATUS(status);
    status = reg_total_number->keyReset(reg_total_number_index.get());
    CHECK_BF_STATUS(status);
    status = reg_total_number->dataReset(reg_total_number_value.get());
    CHECK_BF_STATUS(status);

    std::ofstream outfile("dyso.log");

    std::vector<uint64_t> reg_data_vector;
    // uint32_t round = 0;
    // auto record_last = std::chrono::system_clock::now();
    // auto print_last = std::chrono::system_clock::now();
    auto init_time = std::chrono::system_clock::now();
    
    const uint32_t nGridSize = 10;
    uint32_t nGridToRecord = nGridSize;

    while (true) {
        auto now = std::chrono::system_clock::now();

        /**
         * For fine-grained measurements
         */
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - init_time).count();
        if (elapsed > nGridToRecord) {
            nGridToRecord += nGridSize;
            // read reg_total_number and reg_hit_number
            reg_data_vector.clear();
            status = reg_hit_number_index->setValue(reg_hit_number_key_id, static_cast<uint64_t>(0));
            CHECK_BF_STATUS(status);  // set index to read
            status = reg_hit_number->dataReset(reg_hit_number_value.get());
            CHECK_BF_STATUS(status);
            status = reg_hit_number->tableEntryGet(*session, dev_tgt, *reg_hit_number_index, fromHwFlag, reg_hit_number_value.get());
            CHECK_BF_STATUS(status);
            status = reg_hit_number_value->getValue(reg_hit_number_data_id, &reg_data_vector);
            CHECK_BF_STATUS(status);                   // dump to user-program
            auto req_hit_number = reg_data_vector[0];  // first of reg-pair
            status = session->sessionCompleteOperations();
            CHECK_BF_STATUS(status);

            reg_data_vector.clear();
            status = reg_total_number_index->setValue(reg_total_number_key_id, static_cast<uint64_t>(0));  // set index to read
            status = reg_total_number->dataReset(reg_total_number_value.get());
            status = reg_total_number->tableEntryGet(*session, dev_tgt, *reg_total_number_index, fromHwFlag, reg_total_number_value.get());
            status = reg_total_number_value->getValue(reg_total_number_data_id, &reg_data_vector);  // dump to user-program
            auto req_total_number = reg_data_vector[0];                                             // first of reg-pair
            reg_data_vector.clear();

            outfile << elapsed
                    << "," << req_total_number
                    << "," << req_hit_number
                    << "," << double(req_hit_number) / req_total_number << std::endl;

            // initialize record registers in ASIC
            status = reg_hit_number->tableClear(*session, dev_tgt);
            CHECK_BF_STATUS(status);
            status = reg_total_number->tableClear(*session, dev_tgt);
            CHECK_BF_STATUS(status);
        }

        status = session->sessionCompleteOperations();
        CHECK_BF_STATUS(status);

    }

    // printf("\n\n *** Press any key to exit... *** \n");
    // getchar();

    // /* Run Indefinitely */
    // printf("Run indefinitely...\n");
    // while (true) {
    //     sleep(1);
    // }

    status = session->sessionDestroy();
    CHECK_BF_STATUS(status);
    return status;
}

/* Helper function to check bf_status */
void __check_bf_status__(bf_status_t status, const char *file, int lineNumber) {
    ;
    if (status != BF_SUCCESS) {
        printf("ERROR: CHECK_BF_STATUS failed at %s:%d\n", file, lineNumber);
        printf("   ==> with error: %s\n", bf_err_str(status));
        exit(status);
    }
}

bf_switchd_context_t *init_switchd() {
    bf_status_t status = 0;
    bf_switchd_context_t *switchd_ctx;

    /* Allocate switchd context */
    if ((switchd_ctx = (bf_switchd_context_t *)calloc(
             1, sizeof(bf_switchd_context_t))) == NULL) {
        printf("Cannot Allocate switchd context\n");
        exit(1);
    }

    /* Minimal switchd context initialization to get things going */
    switchd_ctx->install_dir = strdup(SDE_INSTALL);
    switchd_ctx->conf_file = strdup(CONF_FILE_PATH(PROG_NAME));
    switchd_ctx->running_in_background = true;
    switchd_ctx->dev_sts_thread = true;
    switchd_ctx->dev_sts_port = INIT_STATUS_TCP_PORT;

    /* Initialize the device */
    status = bf_switchd_lib_init(switchd_ctx);
    if (status != BF_SUCCESS) {
        printf("ERROR: Device initialization failed: %s\n", bf_err_str(status));
        exit(1);
    }

    return switchd_ctx;
}

int main(int argc, char **argv) {
    /* Not using cmdline params in this minimal boiler plate */
    (void)argc;
    (void)argv;

    bf_status_t status = 0;
    bf_switchd_context_t *switchd_ctx;

    /* Check if this CP program is being run as root */
    if (geteuid() != 0) {
        printf("ERROR: This control plane program must be run as root (e.g. sudo %s)\n", argv[0]);
        exit(1);
    }

    /* Initialize the switchd context */
    switchd_ctx = init_switchd();

    status = app_dyso(switchd_ctx);
    CHECK_BF_STATUS(status);

    if (switchd_ctx) {
        free(switchd_ctx);
    }
    return status;
}
