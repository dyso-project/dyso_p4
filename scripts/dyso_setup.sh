#! /bin/bash

# port setup
$SDE/run_bfshell.sh -f `pwd`/bootstrap/port_setup

# config pkt_gen
~/ica-tools_2022-05-18/run_pd_rpc.py `pwd`/control/dyso/config_pktgen_query.py
~/ica-tools_2022-05-18/run_pd_rpc.py `pwd`/control/dyso/config_pktgen_control.py