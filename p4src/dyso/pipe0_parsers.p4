parser Pipe0SwitchIngressParser(packet_in        pkt,
    /* User */    
    out pipe_0_ingress_headers_t          hdr,
    out pipe_0_ingress_metadata_t         meta,
    /* Intrinsic */
    out ingress_intrinsic_metadata_t  ig_intr_md)
{
    /* This is a mandatory state, required by Tofino Architecture */
     state start {
        pkt.extract(ig_intr_md);

        /* init metadata */
        meta.register_index = 0;
        meta.register_stage = 0;
        meta.query_key = 0;
        meta.query_offset = 0;

        pkt.advance(PORT_METADATA_SIZE);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_PGEN_0 : parse_ipv4;
            ETHERTYPE_CTRL : accept;
            default : reject;
        }
    }
    
    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition accept;
    }   
}

control Pipe0SwitchIngressDeparser(packet_out pkt,
    /* User */
    inout pipe_0_ingress_headers_t                       hdr,
    in    pipe_0_ingress_metadata_t                      meta,
    /* Intrinsic */
    in    ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md)
{
    apply {
        pkt.emit(hdr);
    }
}

parser Pipe0SwitchEgressParser(packet_in        pkt,
    /* User */
    out pipe_0_egress_headers_t          hdr,
    out pipe_0_egress_metadata_t         meta,
    /* Intrinsic */
    out egress_intrinsic_metadata_t  eg_intr_md)
{
    /* This is a mandatory state, required by Tofino Architecture */
    state start {
        pkt.extract(eg_intr_md);
        transition accept;
    }
}

control Pipe0SwitchEgressDeparser(packet_out pkt,
    /* User */
    inout pipe_0_egress_headers_t                       hdr,
    in    pipe_0_egress_metadata_t                      meta,
    /* Intrinsic */
    in    egress_intrinsic_metadata_for_deparser_t  eg_dprsr_md)
{
    apply {
        pkt.emit(hdr);
    }
}
