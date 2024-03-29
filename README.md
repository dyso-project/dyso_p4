# DySO-P4 Testbed Implementation
This repository is a prototype of [DySO](https://github.com/ChahwanSong/ChahwanSong.github.io/blob/main/papers/DySO_IFIP2022.pdf) paper in IFIP 2022 on Intel Tofino programmable switch.

:information_source: This repository is uploaded for the purpose of reference, instead of actual-running code in practice. 

## Requirements
We run this prototype under the following environments:
* SDE-9.2.0 (-> Makefile may not work over SDE-9.7.0. See [control/dyso/cpp/Makefile](https://github.com/dyso-project/dyso_p4/blob/main/control/dyso/cpp/Makefile).)
* PcapPlusPlus v22.XX atop DPDK v20.11 (-> See [control/dyso/pcpp](https://github.com/dyso-project/dyso_p4/tree/main/control/dyso/pcpp))

### :exclamation: Recent Update (30 Nov, 2022)
:bulb: We updated our code to be runnable on SDE >= 9.7.0. Note that we leverage multi-pipeline (2 pipes) Tofino ASIC. Unfortunately, we found the recent SDE has an issue of multi-pipeline p4 programs (also discussed in Intel Connectivity Academy Forum). For example, your p4 program would not find any valid ports or drivers. 
To handle this, please follow the instructions:
1. Compile dyso.p4.
2. Update the configuration file in the directory `$SDE/install/share/p4/targets/tofino/dyso.conf`, remove pipe 2 and 3.
3. After two steps, you can run the program, i.e., `./run_switch_d -p dyso`, or run the bash scripts we provide. 


## How To Run DySO

After compiling dyso control program written in pcpp (`control/dyso/pcpp/Makefile`) and switch's cpp program (`control/dyso/cpp/Makefile`), run two python programs on server and switch, respectively:
1. Run `python3 dyso_simulation_server.py` at server, then
2. Run `python3 dyso_simulation_switch.py` at switch.

### Query generator
![QueryGen](/misc/querygen.jpg)
We generate queries via Tofino traffic generator (pipe 0). It generates 80 Mpps query stream with shifting popularity ranks (see the evaluation part of paper) in following steps:
1. From [misc/zipf.txt](https://github.com/dyso-project/dyso_p4/blob/main/misc/zipf.txt), the switch's register array (size 524288) reads the flowIDs (4 Bytes) and save them. The flowIDs are ranging from 1 to N (e.g., 138092), where N is the number of flows. Here, we use zipf(0.99) distribution.
2. There is an offset register X, by default 0. For every generated packet, it samples randomly from the above registers. Once it samples one register with flowID=Y, the packet's final key is X+Y (or X-Y).
3. To shift the popularity of content, we adjust the offset X. We periodically increase the offset value to generate dynamic workloads.
For simplicity, in this prototype, we generate key-matching queries (key size = 4 Bytes) with no values.
Please see [control/dyso/config_pktgen_query.py](https://github.com/dyso-project/dyso_p4/blob/main/control/dyso/config_pktgen_query.py) to change the rate. 

### Control packet generator

Similar to query generator, we generate control packets via Tofino traffic generator (pipe 1). 
Please see [control/dyso/config_pktgen_control.py](https://github.com/dyso-project/dyso_p4/blob/main/control/dyso/config_pktgen_control.py) to change the rate. 

## Running DySO control loop

### P4 control plane
The file [control/dyso/cpp/dyso.cpp](https://github.com/dyso-project/dyso_p4/blob/main/control/dyso/cpp/dyso.cpp) is the runtime interface code written in C++. 
:bulb: We updated our code to be runnable on SDE >= 9.7.0. To run the bfrt c++, use [control/dyso/cpp/run.sh](https://github.com/dyso-project/dyso_p4/blob/main/control/dyso/cpp/run.sh).


### P4 source code
P4 source code is in [p4src/dyso](https://github.com/dyso-project/dyso_p4/tree/main/p4src/dyso). 
Note that the key-matching and lookup-history storage codes are implemented in pipeline1, whereas pipeline0 performs query generator with key allocation.


### DySO's policy data structure
In folder [control/dyso/pcpp/src](https://github.com/dyso-project/dyso_p4/tree/main/control/dyso/pcpp/src), there are scripts implementing the policy data structure (see the paper) and other utility files such as lock-free queue (MoodyCamel) and efficient software hash table (RobinHood). 


### PcapPlusPlus source code
In folder [control/dyso/pcpp](https://github.com/dyso-project/dyso_p4/tree/main/control/dyso/pcpp), there are codes for PcapPlusPlus threading with DPDK custom packet header parsers. 



### Miscellaneous
- We set a default register value as `7777777` to identify an default/empty register value. Refer to the file `control/dyso/debug/set_key_default.py`.
- Hardcoded constants can be found in `p4src/dyso/constants.p4`.


## :exclamation: Issues & Solutions

### PcapPlusPlus compile error by `-std=c++11`
From PcapPlusPlus version 22, Change the compiler version to c++17 by default. 
Change the line 
```
PCAPPP_BUILD_FLAGS := -fPIC -std=c++11
``` 
to 
```
PCAPPP_BUILD_FLAGS := -fPIC -std=c++17
``` 
from the script `/usr/local/etc/PcapPlusPlus.mk`.


### Using multiple cores for DPDK Rx/Tx
Currently, our prototype uses only ONE DPDK RX and ONE DPDK TX core, for simplicity.
This is because our control packet rate is under 10Gbps, which can be managed using a single core.
If you want to scale up, you may need to adjust the PcapPlusPlus code.



### PcapPlusPlus port mapping
DySo Pcap++ program automatically binds first two DPDK ports. So, you need to correctly forward the control packets from data plane to the port running `StatModule`.
The easiest way is to do brute-force. Just let program run, and if the StatModule does not receive any packets, then swap the port numbers and run again. 
The port number is assigned in `control/dyso/pcpp/src/utils_pcpp.h`:
```
#define DEVICE_ID_UPDATE (0)
#define DEVICE_ID_STAT (1)
```


### Port cabling of control plane to external-server 
Ensure to cable Tofino front-panel port connected to your external-server based control plane with pipeline 0.
This is because the control packets are recirculated to pipeline 1, and only the recirculated (control) packets are processed in pipeline 1.
Otherwise, DySO state management module gives an error message like `the received packet might be from the previous experiment`. 


## Contact
Please contact to Chahwan Song ([skychahwan@gmail.com](skychahwan@gmail.com))