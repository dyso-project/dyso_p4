#!/usr/bin/python
# -*- coding: utf-8 -*-

import os
import sys

if os.getuid() != 0:
    print("ERROR : This script requires root privileges. use 'sudo' to run it. ")
    quit()

from scapy.all import sendp, send, get_if_list, get_if_hwaddr, hexdump
from scapy.all import Packet
from scapy.all import Ether, IP, UDP, TCP
from scapy.all import hexdump, BitField, BitFieldLenField, ShortEnumField, X3BytesField, ByteField, XByteField
from scapy.all import *

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

def sendControl(idx_update, iface):
    controlPkt = Ether(type=0xDEAD) / control_packet(index_update=idx_update,
                                                    key0=5,
                                                    key1=6,
                                                    key2=7,
                                                    key3=8,
                                                    index_probe=idx_update,
                                                    rec0=0,
                                                    rec1=0,
                                                    rec2=0,
                                                    rec3=0,
                                                    rec4=0,
                                                    rec5=0,
                                                    rec6=0,
                                                    rec7=0)
    sendp(controlPkt, iface=iface, verbose=False)

def main():
    iface = 'ens1f1'
    print('---------- Send pakcets ----------')
    # for i in range(256):
    sendControl(1, iface)

if __name__ == '__main__':
    main()
