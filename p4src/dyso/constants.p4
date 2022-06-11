const ether_type_t ETHERTYPE_IPV4 = 0x0800;     // ipv4
const ether_type_t ETHERTYPE_PGEN_0 = 0xBFBF;   // pktgen pipe0 - query
const ether_type_t ETHERTYPE_PGEN_1 = 0xFBFB;   // pktgen pipe1 - control
const ether_type_t ETHERTYPE_CTRL = 0xDEAD;      // dyso control

#define PGEN_PORT_0 68
#define PGEN_PORT_1 196

#define REG_WIDTH 32

#define RECIRC_PORT_0 128
#define RECIRC_PORT_1 132

#define REG_LEN 131072
#define REG_LEN_KEY_IDX 14
#define REG_LEN_KEY (1<<14)

#define REG_LEN_REC_IDX 8
#define REG_LEN_REC (1<<8)
const bit<32> RoundRobinTheta = 0xFF;

#define REG_KEY(num) \
Register<bit<REG_WIDTH>, _>(REG_LEN_KEY) key##num##; \
RegisterAction<bit<REG_WIDTH>, _, bit<1>>(key##num) key##num##_read = {\
    void apply(inout bit<REG_WIDTH> val, out bit<1> rv){\
        if(meta.match_key == val) {\
            rv = 1;\
        } else { \
            rv = 0; \
        }\
    }\
};\
RegisterAction<bit<REG_WIDTH>, _, bit<REG_WIDTH>>(key##num##) key##num##_update = {\
    void apply(inout bit<REG_WIDTH> val){\
        val = meta.update_key##num##;\
    }\
};

#define REG_REC(num) \
Register<bit<REG_WIDTH>, _>(REG_LEN_REC) rec##num##; \
RegisterAction<bit<REG_WIDTH>, _, bit<REG_WIDTH>>(rec##num##) rec##num##_read_and_clear = {\
    void apply(inout bit<REG_WIDTH> val, out bit<REG_WIDTH> rv){\
        rv = val; \
        val = 0; \
    }\
};\
RegisterAction<bit<REG_WIDTH>, _, bit<1>>(rec##num##) rec##num##_update = {\
    void apply(inout bit<REG_WIDTH> val, out bit<1> rv){\
        if(val == 0) {  \
            val = meta.packet_sig; \
            rv = 1; \
        } else {    \
            rv = 0; \
        }   \
    }\
};
