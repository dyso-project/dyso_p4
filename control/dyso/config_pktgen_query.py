from scapy.all import *
import time

# Pipeline 0 ==> Query packets
ETHERTYPE = 0xBFBF  # used to identify pktgen pkt
SRC_MAC = "BF:CC:11:22:33:44"  # doesn't matter
PKT_LENGTH = 64 # 60, 124, 508, 1020, 1514

TIME_PERIOD = 12  # 10ns -> 100 Mpps
# TIME_PERIOD = 1000000000  # 1s

def make_port(pipe, local_port):
    assert pipe >= 0 and pipe < 4
    assert local_port >= 0 and local_port < 72
    return pipe << 7 | local_port

PKTS_COUNT = 1

# FORM a TCP packet to be written to buffer
pkt = Ether(dst="01:02:03:04:05:06", src=SRC_MAC, type=ETHERTYPE)
pkt = pkt / IP() / TCP()
pkt = pkt / Raw(load=bytearray(PKT_LENGTH - len(pkt)))

# hexdump(pkt)

# Write the packet to the pktgen buffer
# skip the first 6 bytes for pktgen header
pktgen.write_pkt_buffer(0, len(pkt) - 6, bytes(pkt)[6:]) # buffer offset, buffer size, buffer data

# enable pktgen on pipe 0's port 68 (100Gbps)
pktgen.enable(make_port(0, 68))  # port 68

# create the app configuration
app_cfg = pktgen.AppCfg_t()
app_cfg.trigger_type = pktgen.TriggerType_t.TIMER_PERIODIC
app_cfg.timer = TIME_PERIOD 
app_cfg.batch_count = 0 # sets no. of batches that we want to have; the batch_id field of pktgen header keeps incrementing until this value is reached
app_cfg.pkt_count = PKTS_COUNT - 1 # sets no. of packets that we want to have in a batch; the packet_id field of pktgen header keeps incrementing until this value is reached. We are doing -1 in the above case because the numbering is starting from 0. pkt_count = 0 means 1 pkt per batch and batch_count = 0 means 1 batch per trigger
app_cfg.src_port = 68   # pipe local src port
app_cfg.buffer_offset = 0
app_cfg.length = len(pkt) - 6 

# configure app id 1 with the app config
pktgen.cfg_app(1, app_cfg)
conn_mgr.complete_operations()

###################################################################################################

# -------------------- START PKTGEN TRAFFIC-------------- #
# pktgen.app_enable(1)
# print("PktGen Traffic Started")
# pktgen.app_disable(1)
# pktgen.show_counters(same=True)
