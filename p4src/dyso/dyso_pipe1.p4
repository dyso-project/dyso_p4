#include "pipe1_headers.p4"
#include "pipe1_parsers.p4"

control Pipe1SwitchIngress(
    /* User */
    inout pipe_1_ingress_headers_t                       hdr,
    inout pipe_1_ingress_metadata_t                      meta,
    /* Intrinsic */
    in    ingress_intrinsic_metadata_t               ig_intr_md,
    in    ingress_intrinsic_metadata_from_parser_t   ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t  ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t        ig_tm_md)
{
    /* forward to egress port */
    action forward(bit<9> port) {
        ig_tm_md.ucast_egress_port = port;
        ig_tm_md.bypass_egress = 1;
    }

    action drop(){
		ig_dprsr_md.drop_ctl = 0x1; // Drop packet.
	}
    
    /* hash functions */
    CRCPolynomial<bit<32>>(32w0x04C11DB7, 
                           false, 
                           false, 
                           false, 
                           32w0xFFFFFFFF,
                           32w0x00000000
                           ) CRC32_MPEG;

    Hash<bit<32>>(HashAlgorithm_t.CRC32) hash_crc32;
    Hash<bit<32>>(HashAlgorithm_t.CUSTOM, CRC32_MPEG) hash_crc32_mpeg;

    action get_crc32() {
        meta.hash_crc32 = hash_crc32.get({ hdr.ipv4.src_addr });
    }
    action get_crc32_mpeg() {
        meta.hash_crc32_mpeg = hash_crc32_mpeg.get({ hdr.ipv4.src_addr });
    }
    action get_indices() {
        meta.key_idx = meta.hash_crc32_mpeg[13:0];
        meta.rec_idx = meta.hash_crc32_mpeg[13:6];
    }
    action get_packet_sig() {
        meta.packet_sig = meta.hash_crc32_mpeg[5:0] ++ meta.hash_crc32[25:0];
    }
    action copy_match() {
        meta.match_key = hdr.ipv4.src_addr;
    }
    action copy_update() {
        meta.update_idx = hdr.ctrl.index_update; // idx to update keys in RAs
        meta.update_key0 = hdr.ctrl.key0;
        meta.update_key1 = hdr.ctrl.key1;
        meta.update_key2 = hdr.ctrl.key2;
        meta.update_key3 = hdr.ctrl.key3;
    }

    // round-robin of probing
    Register<bit<32>, bit<1>>(1) reg_idx_to_probe;
    RegisterAction<bit<32>, bit<1>, bit<32>>(reg_idx_to_probe) reg_idx_to_probe_action = {
        void apply(inout bit<32> value, out bit<32> result) {
            /* if value >= nRepoEntry - 1 */
            if (value >= RoundRobinTheta) {
                value = 0;
            } else {
                value = value + 1; // round-robin, modular to nEntry
            }
            result = value;
        }
    };
    action copy_probe() {
        meta.probe_idx = reg_idx_to_probe_action.execute(0);
    }
    action copy_probe_idx_to_hdr() {
        hdr.ctrl.index_probe = meta.probe_idx;
    }

    Register<bit<32>, bit<1>>(1) reg_check_update_idx;
    RegisterAction<bit<32>, bit<1>, bit<32>>(reg_check_update_idx) reg_check_update_idx_action = {
        void apply(inout bit<32> value, out bit<32> result) {
            if (hdr.ctrl.index_update >= 32w7777777) {
                result = 0;
            } else {
                result = 1;
            }
        }
    };
    action check_update_dummy() {
        meta.real_update = (bit<1>)(reg_check_update_idx_action.execute(0));
    }

    /* START dyso declare the registers */
    REG_KEY(0)
    REG_KEY(1)
    REG_KEY(2)
    REG_KEY(3)

    REG_REC(0)
    REG_REC(1)
    REG_REC(2)
    REG_REC(3)
    REG_REC(4)
    REG_REC(5)
    REG_REC(6)
    REG_REC(7)
    /* END dyso register declaration */

    action action_key0_update() {
        key0_update.execute(meta.update_idx);
    }
    action action_key1_update() {
        key1_update.execute(meta.update_idx);
    }
    action action_key2_update() {
        key2_update.execute(meta.update_idx);
    }
    action action_key3_update() {
        key3_update.execute(meta.update_idx);
    }

    action action_rec0_read_and_clear() {
        hdr.ctrl.rec0 = rec0_read_and_clear.execute(meta.probe_idx);
    }
    action action_rec1_read_and_clear() {
        hdr.ctrl.rec1 = rec1_read_and_clear.execute(meta.probe_idx);
    }
    action action_rec2_read_and_clear() {
        hdr.ctrl.rec2 = rec2_read_and_clear.execute(meta.probe_idx);
    }
    action action_rec3_read_and_clear() {
        hdr.ctrl.rec3 = rec3_read_and_clear.execute(meta.probe_idx);
    }
    action action_rec4_read_and_clear() {
        hdr.ctrl.rec4 = rec4_read_and_clear.execute(meta.probe_idx);
    }
    action action_rec5_read_and_clear() {
        hdr.ctrl.rec5 = rec5_read_and_clear.execute(meta.probe_idx);
    }
    action action_rec6_read_and_clear() {
        hdr.ctrl.rec6 = rec6_read_and_clear.execute(meta.probe_idx);
    }
    action action_rec7_read_and_clear() {
        hdr.ctrl.rec7 = rec7_read_and_clear.execute(meta.probe_idx);
    }

    Register<bit<32>, bit<1>>(1) reg_hit_number;
    RegisterAction<bit<32>, bit<1>, bit<32>>(reg_hit_number) reg_hit_number_action = {
        void apply(inout bit<32> value){
            value = value + 1;
        }
    };

    Register<bit<32>, bit<1>>(1) reg_total_number;
    RegisterAction<bit<32>, bit<1>, bit<32>>(reg_total_number) reg_total_number_action = {
        void apply(inout bit<32> value){
            value = value + 1;
        }
    };


    apply {
        // from pipe0 traffic generator
        // todo: if need to scale up the traffic, then we need to do multicast - check whether the packet was a multicast packet
        if(ig_intr_md.ingress_port == RECIRC_PORT_0 && hdr.ipv4.isValid()) {    
            // todo: read the registers, and store signature
            get_crc32();
            get_crc32_mpeg();
            get_indices();
            get_packet_sig();
            reg_total_number_action.execute(0);
            copy_match();

            // cache matching here
            // first stage - always try
            // return 0 if miss, 1 if hit 
            meta.key0_hit = key0_read.execute(meta.key_idx);
            if(meta.key0_hit == 0) {
                meta.key1_hit = key1_read.execute(meta.key_idx);
            } else if(meta.key0_hit == 1)  {
                meta.cache_hit = 1;
            }

            if(meta.key1_hit == 0) {
                meta.key2_hit = key2_read.execute(meta.key_idx);
            } else if(meta.key1_hit == 1) {
                meta.cache_hit = 1;
            }

            if(meta.key2_hit == 0) {
                meta.key3_hit = key3_read.execute(meta.key_idx);
            } else if(meta.key2_hit == 1) {
                meta.cache_hit = 1;
            }

            if(meta.key3_hit == 1) {
                meta.cache_hit = 1;
            }


            // record packet signature here
            // first stage - always try insert
            meta.rec0_ins = rec0_update.execute(meta.rec_idx);
            bit<1> tmp0 = 0;
            if(meta.rec0_ins == 0) {
                meta.rec1_ins = rec1_update.execute(meta.rec_idx);
            } else {
                tmp0 = 1;
            }

            bit<1> tmp1 = 0;
            if(meta.rec1_ins == 0 && tmp0 == 0) {
                meta.rec2_ins = rec2_update.execute(meta.rec_idx);
            } else {
                tmp1 = 1;
            }

            bit<1> tmp2 = 0;
            if(meta.rec2_ins == 0 && tmp1 == 0) {
                meta.rec3_ins = rec3_update.execute(meta.rec_idx);
            } else {
                tmp2 = 1;
            }

            bit<1> tmp3 = 0;
            if(meta.rec3_ins == 0 && tmp2 == 0) {
                meta.rec4_ins = rec4_update.execute(meta.rec_idx);
            } else {
                tmp3 = 1;
            }
            
            bit<1> tmp4 = 0;
            if(meta.rec4_ins == 0 && tmp3 == 0) {
                meta.rec5_ins = rec5_update.execute(meta.rec_idx);
            } else {
                tmp4 = 1;
            }

            bit<1> tmp5 = 0;
            if(meta.rec5_ins == 0 && tmp4 == 0) {
                meta.rec6_ins = rec6_update.execute(meta.rec_idx);
            } else {
                tmp5 = 1;
            }

            bit<1> tmp6 = 0;
            if(meta.rec6_ins == 0 && tmp5 == 0) {
                meta.rec7_ins = rec7_update.execute(meta.rec_idx);
            } else {
                tmp6 = 1;
            }

            if (meta.cache_hit == 1) {
                reg_hit_number_action.execute(0);
            }

            // drop the query packet
            drop();
        } 
        // from pipe1 traffic generator
        else if(ig_intr_md.ingress_port == PGEN_PORT_1 && hdr.ethernet.ether_type == ETHERTYPE_PGEN_1) {
            hdr.ethernet.ether_type = ETHERTYPE_CTRL;
            ig_tm_md.ucast_egress_port = 64;
            ig_tm_md.bypass_egress = 1;
        } 
        // from port64 (control plane)
        else if(ig_intr_md.ingress_port == RECIRC_PORT_1 && hdr.ethernet.ether_type == ETHERTYPE_CTRL) {
            // todo: update keys and copy record
            check_update_dummy();
            copy_update();
            copy_probe();
            
            if (meta.real_update == 1) {
                action_key0_update();
            }
            if (meta.real_update == 1) {
                action_key1_update();
            }
            if (meta.real_update == 1) {
                action_key2_update();
            }
            if (meta.real_update == 1) {
                action_key3_update();
            }

            copy_probe_idx_to_hdr();
            action_rec0_read_and_clear();
            action_rec1_read_and_clear();
            action_rec2_read_and_clear();
            action_rec3_read_and_clear();
            action_rec4_read_and_clear();
            action_rec5_read_and_clear();
            action_rec6_read_and_clear();
            action_rec7_read_and_clear();
            
            ig_tm_md.ucast_egress_port = 65;
            ig_tm_md.bypass_egress = 1;
        }
    }
}

control Pipe1SwitchEgress(
    /* User */
    inout pipe_1_egress_headers_t                          hdr,
    inout pipe_1_egress_metadata_t                         meta,
    /* Intrinsic */    
    in    egress_intrinsic_metadata_t                  eg_intr_md,
    in    egress_intrinsic_metadata_from_parser_t      eg_prsr_md,
    inout egress_intrinsic_metadata_for_deparser_t     eg_dprsr_md,
    inout egress_intrinsic_metadata_for_output_port_t  eg_oport_md)
{
    apply {}
}