import sys
import os
import argparse

sde_install = os.environ['SDE_INSTALL']
sys.path.append('%s/lib/python2.7/site-packages/tofino'%(sde_install))
sys.path.append('%s/lib/python2.7/site-packages/p4testutils'%(sde_install))
sys.path.append('%s/lib/python2.7/site-packages'%(sde_install))

import grpc
import time
from pprint import pprint
import bfrt_grpc.client as gc
import bfrt_grpc.bfruntime_pb2 as bfruntime_pb2


def connect():
    # Connect to BfRt Server
    interface = gc.ClientInterface(grpc_addr='localhost:50052', client_id=0, device_id=0)
    target = gc.Target(device_id=0, pipe_id=0xffff)
    # print('Connected to BfRt Server!')

    # Get the information about the running program
    bfrt_info = interface.bfrt_info_get()

    # Establish that you are working with this program
    interface.bind_pipeline_config(bfrt_info.p4_name_get())
    return interface, target, bfrt_info

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--offset', type=int, default=0, required=True,
        help='specify the query key offset to simulate popularity changes'
    )

    args = parser.parse_args()
    offset = int(args.offset)

    interface, target, bfrt_info = connect()

    reg_offset = bfrt_info.table_get('Pipe0SwitchIngress.offset')
    key = [reg_offset.make_key([gc.KeyTuple('$REGISTER_INDEX', 0)])]
    data = [reg_offset.make_data([gc.DataTuple('Pipe0SwitchIngress.offset.f1', offset)])]
    reg_offset.entry_mod(target, key, data)

    print "!! Offset set to ==>", offset


if __name__ == '__main__':
    main()
