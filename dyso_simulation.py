# /usr/bin/python3

import subprocess
import os
import time

####### your passowrd of SUDO privilege #######
pwd='yourSudoPassword'
###############################################

print("*** Remove /dev/shm/ files for sanity")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo rm /dev/shm/*'), shell=True)
print("->done\n")

print("*** Run DySO's control plane with bfrt (wait 40 seconds)")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/cpp/dyso &'), shell=True)
time.sleep(40)
print("->done\n")

print("*** Start setup DySO Workers (wait 20 seconds)")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 0 &'), shell=True)
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 1 &'), shell=True)
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 2 &'), shell=True)
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 3 &'), shell=True)
time.sleep(20)
print("->done\n")

print("*** Start Pcap++ DPDK Stat/Update Engines (wait 40 seconds)")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/pcpp_dyso.o &'), shell=True)
time.sleep(40)
print("->done\n")

print("*** Start setup dyso's ports")
subprocess.run(["bash", "./scripts/dyso_setup.sh"])
print("*** Loading register values for query generation")
subprocess.run(["bash", "./scripts/dyso_load_data.sh"])
print("*** Start control and query packet generators")
subprocess.run(["bash", "./scripts/dyso_start_pktgen_control.sh"])
subprocess.run(["bash", "./scripts/dyso_start_pktgen_query.sh"])
print("->done\n")

print("Start measurement...")
# variable (speed of evolution of popularity)
interval_size = 5
offset_size = 1000 # offset size
total_interval = 100 # 100 seconds

# local const
round = 0 # round
while (round < int(total_interval / interval_size)):
    time.sleep(interval_size)
    round = round + 1
    offset = round * offset_size
    print("offset: ", offset)
    subprocess.run(["bash", "./scripts/dyso_change_offset.sh", str(offset)])
    
# # print("*** Any input to stop generators ***")
# # str(input())

time.sleep(5)
subprocess.run(["bash", "./scripts/dyso_stop_pktgen_control.sh"])
subprocess.run(["bash", "./scripts/dyso_stop_pktgen_query.sh"])
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'pkill -f dyso'), shell=True)
