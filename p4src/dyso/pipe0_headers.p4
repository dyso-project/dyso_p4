struct pipe_0_ingress_headers_t {
    ethernet_h      ethernet;
    ipv4_h          ipv4;
}

struct pipe_0_ingress_metadata_t {
    bit<2>      register_stage;
    bit<17>     register_index;

    bit<32>     query_key;
    bit<32>     query_offset;
}

struct pipe_0_egress_headers_t {

}

struct pipe_0_egress_metadata_t {
}