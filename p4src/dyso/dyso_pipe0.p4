#include "pipe0_headers.p4"
#include "pipe0_parsers.p4"

control Pipe0SwitchIngress(
    /* User */
    inout pipe_0_ingress_headers_t                       hdr,
    inout pipe_0_ingress_metadata_t                      meta,
    /* Intrinsic */
    in    ingress_intrinsic_metadata_t               ig_intr_md,
    in    ingress_intrinsic_metadata_from_parser_t   ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t        ig_tm_md)
{

    Register<bit<REG_WIDTH>, _>(REG_LEN) d0;
    RegisterAction<bit<REG_WIDTH>, _, bit<REG_WIDTH>>(d0) read_d0 = {
        void apply(inout bit<REG_WIDTH> val, out bit<REG_WIDTH> rv){
            bit<32> tmp;
            tmp = val - meta.query_offset;

            rv = tmp;
        }
    };

    Register<bit<REG_WIDTH>, _>(REG_LEN) d1;
    RegisterAction<bit<REG_WIDTH>, _, bit<REG_WIDTH>>(d1) read_d1 = {
        void apply(inout bit<REG_WIDTH> val, out bit<REG_WIDTH> rv){
            bit<32> tmp;
            tmp = val - meta.query_offset;

            rv = tmp;
        }
    };

    Register<bit<REG_WIDTH>, _>(REG_LEN) d2;
    RegisterAction<bit<REG_WIDTH>, _, bit<REG_WIDTH>>(d2) read_d2 = {
        void apply(inout bit<REG_WIDTH> val, out bit<REG_WIDTH> rv){
            bit<32> tmp;
            tmp = val - meta.query_offset;

            rv = tmp;
        }
    };

    Register<bit<REG_WIDTH>, _>(REG_LEN) d3;
    RegisterAction<bit<REG_WIDTH>, _, bit<REG_WIDTH>>(d3) read_d3 = {
        void apply(inout bit<REG_WIDTH> val, out bit<REG_WIDTH> rv){
            bit<32> tmp;
            tmp = val - meta.query_offset;

            rv = tmp;
        }
    };

    Register<bit<REG_WIDTH>, _>(1) offset;
     RegisterAction<bit<REG_WIDTH>, _, bit<REG_WIDTH>>(offset) read_offset = {
        void apply(inout bit<REG_WIDTH> val, out bit<REG_WIDTH> rv){
            rv = val;
        }
    };

    Random<bit<2>>() rng0;
    action get_rand_stage() {
        meta.register_stage = rng0.get();
    }

    Random<bit<17>>() rng1;
    action get_rand_index() {
        meta.register_index = rng1.get();
    }

    apply {
        if(ig_intr_md.ingress_port == PGEN_PORT_0 && hdr.ipv4.isValid()) {
            get_rand_stage();
            get_rand_index();

            meta.query_offset = read_offset.execute(0);

            if(meta.register_stage == 0) {
                meta.query_key = read_d0.execute(meta.register_index);
            } else if(meta.register_stage == 1) {
                meta.query_key = read_d1.execute(meta.register_index);
            } else if(meta.register_stage == 2) {
                meta.query_key = read_d2.execute(meta.register_index);
            } else if(meta.register_stage == 3) {
                meta.query_key = read_d3.execute(meta.register_index);
            }

            hdr.ipv4.src_addr = meta.query_key;
            hdr.ethernet.ether_type = ETHERTYPE_IPV4;

            // to pipeline 1 - recirculate via 128 (front panel port 57)
            // TODO: if need scale up with more traffic, then use multicast and put more ports at pipeline 1 in loopback mode
            ig_tm_md.ucast_egress_port = RECIRC_PORT_0;
            ig_tm_md.bypass_egress = 1;
        } 
        // control packet coming back from port 64 and must be control packet
        else if (ig_intr_md.ingress_port == 64 && hdr.ethernet.ether_type == ETHERTYPE_CTRL) {
            // to pipeline 1 - recirculate via 132 (front panel port 58)
            ig_tm_md.ucast_egress_port = RECIRC_PORT_1;
            ig_tm_md.bypass_egress = 1;
        }
    }
}

control Pipe0SwitchEgress(
    /* User */
    inout pipe_0_egress_headers_t                          hdr,
    inout pipe_0_egress_metadata_t                         meta,
    /* Intrinsic */    
    in    egress_intrinsic_metadata_t                  eg_intr_md,
    in    egress_intrinsic_metadata_from_parser_t      eg_prsr_md,
    inout egress_intrinsic_metadata_for_deparser_t     eg_dprsr_md,
    inout egress_intrinsic_metadata_for_output_port_t  eg_oport_md)
{
    apply {}
}