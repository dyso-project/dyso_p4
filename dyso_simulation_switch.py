# /usr/bin/python3

import subprocess
import os
import time

####### your passowrd of SUDO privilege #######
pwd='cirlab@123'
###############################################

print("*** Run DySO's control plane with bfrt (+ 30 seconds waiting)")
# subprocess.run(["bash", "./scripts/run.sh"])
subprocess.call('bash {}'.format('./scripts/run.sh &'), shell=True)
time.sleep(30)
print("->done\n")

print("Now, start (1) server's dyso_multicore programs and (2) pcpp_dyso program. Enter anything if you finished running them.")
print("*** Start setup dyso's pktgen configs")
subprocess.run(["bash", "./scripts/dyso_setup.sh"]) # switch
print("*** Loading register values for query generation (taking a few minutes)")
subprocess.run(["bash", "./scripts/dyso_load_data.sh"]) # switch, takes a few (<= 5) minutes
print("*** Start control and query packet generators")



input("==> Enter anything if you want to invoke packet generators (query & control)\n")
subprocess.run(["bash", "./scripts/dyso_start_pktgen_control.sh"])
subprocess.run(["bash", "./scripts/dyso_start_pktgen_query.sh"])
print("->done\n")

# input("==> Enter anything to start popularity changing...")
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

print("*** Kill all programs...")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'pkill -f dyso'), shell=True)
