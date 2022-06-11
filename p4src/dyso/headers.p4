header ethernet_h {
    mac_addr_t      dst_addr;
    mac_addr_t      src_addr;
    ether_type_t    ether_type;
}

header ipv4_h {
    bit<4>   version;
    bit<4>   ihl;
    bit<8>   diffserv;
    bit<16>  total_len;
    bit<16>  identification;
    bit<3>   flags;
    bit<13>  frag_offset;
    bit<8>   ttl;
    ip_proto_t   protocol;
    bit<16>  hdr_checksum;
    ipv4_addr_t  src_addr;
    ipv4_addr_t  dst_addr;
}

header ctrl_h {
    bit<32> index_update;
    bit<32> key0;
    bit<32> key1;
    bit<32> key2;
    bit<32> key3;
    bit<32> index_probe;
    bit<32> rec0;
    bit<32> rec1;
    bit<32> rec2;
    bit<32> rec3;
    bit<32> rec4;
    bit<32> rec5;
    bit<32> rec6;
    bit<32> rec7;
}