#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys

from scapy.all import sendp, send, get_if_list, get_if_hwaddr, hexdump
from scapy.all import Packet
from scapy.all import Ether, IP, UDP, TCP
from scapy.all import hexdump, BitField, BitFieldLenField, ShortEnumField, X3BytesField, ByteField, XByteField
from scapy.layers.inet import _IPOption_HDR
from scapy.all import *

if os.getuid() != 0:
    print("ERROR : This script requires root privileges. use 'sudo' to run it. ")
    quit()


class control_packet(Packet):
    """Control Header (Ehtertype=0xDEAD)"""
    name = "update header"
    fields_desc = [
      BitField("index_update", 0, 32), # idx to update
      BitField("key0", 0, 32),
      BitField("key1", 0, 32),
      BitField("key2", 0, 32),
      BitField("key3", 0, 32),

      BitField("index_probe", 0, 32), # idx to probe
      BitField("rec0", 0, 32),
      BitField("rec1", 0, 32),
      BitField("rec2", 0, 32),
      BitField("rec3", 0, 32),
      BitField("rec4", 0, 32),
      BitField("rec5", 0, 32),
      BitField("rec6", 0, 32),
      BitField("rec7", 0, 32),
    ]


def handle_pkt(pkt):
    a = pkt.show()
    hexdump(pkt)
    print("Packet length : %d\n-------------------------------\n\n\n" % len(pkt))

def main():
    ifaces = os.listdir('/sys/class/net/')
    # ifaces = filter(lambda i: '' in i, ifaces) # print interface

    # iface = 'enp4s0f0' # port 64
    iface = 'enp4s0f1' # port 65
    print("sniffing on %s" % iface)

    # bind layer
    bind_layers(Ether, control_packet)
    sys.stdout.flush()

    sniff(iface = iface, prn = lambda x: handle_pkt(x))

if __name__ == '__main__':
    main()
