struct pipe_1_ingress_headers_t {
    ethernet_h      ethernet;
    ctrl_h          ctrl;
    ipv4_h          ipv4;
}

struct pipe_1_ingress_metadata_t {
    // hash computations
    bit<32>       hash_crc32;
    bit<32>       hash_crc32_mpeg;

    // signature
    bit<32>     packet_sig;

    // indices
    bit<14>     key_idx;
    bit<8>      rec_idx;

    // key
    bit<32> match_key;
    bit<32> update_idx;
    bit<32> update_key0;
    bit<32> update_key1;
    bit<32> update_key2;
    bit<32> update_key3;

    // status
    bit<1>  key0_hit; 
    bit<1>  key1_hit; 
    bit<1>  key2_hit; 
    bit<1>  key3_hit; 
    bit<1>  cache_hit;

    // records
    bit<32> probe_idx;
    bit<1>  rec0_ins; 
    bit<1>  rec1_ins; 
    bit<1>  rec2_ins; 
    bit<1>  rec3_ins; 
    bit<1>  rec4_ins; 
    bit<1>  rec5_ins; 
    bit<1>  rec6_ins; 
    bit<1>  rec7_ins; 
    bit<1>  insert; 

    // check dummy update index
    bit<1> real_update;
}

struct pipe_1_egress_headers_t {
}

struct pipe_1_egress_metadata_t {
}