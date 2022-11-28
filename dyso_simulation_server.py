# /usr/bin/python3

import subprocess
import os
import time

####### your passowrd of SUDO privilege #######
pwd='skychahwan'
###############################################

print("*** Remove /dev/shm/ files for sanity")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo rm /dev/shm/*'), shell=True) # patronus
print("->done\n")

print("*** Start setup DySO Workers (wait 20 seconds)")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 0 &'), shell=True) # patronus
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 1 &'), shell=True) # patronus
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 2 &'), shell=True) # patronus
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/dyso_multicore.o 3 &'), shell=True) # patronus
time.sleep(20)
print("->done\n")

print("*** Start Pcap++ DPDK Stat/Update Engines (+ 40 seconds waiting)")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'sudo ./control/dyso/pcpp/pcpp_dyso.o &'), shell=True) # patronus
time.sleep(40)
print("->done\n")

input("==> Enter anything to kill all programs...\n")
subprocess.call('echo {} | sudo -S {}'.format(pwd, 'pkill -f dyso'), shell=True) # switch / patronus
