# DySO-P4 Testbed Implementation
This repository is a prototype of [DySO](https://github.com/ChahwanSong/ChahwanSong.github.io/blob/main/papers/DySO_IFIP2022.pdf) paper in IFIP 2022 on Intel Tofino programmable switch.

:information_source: This repository is uploaded for the purpose of reference, instead of actual-running code. Indeed, configuring your own programmable switch is non-trivial if no knowledge of your hardware information. 

## Requirements
We run this prototype under the following environments:
* SDE-9.2.0 (-> Makefile may not work over SDE-9.7.0. See [control/dyso/cpp/Makefile](https://github.com/dyso-project/dyso_p4/blob/main/control/dyso/cpp/Makefile).)
* PcapPlusPlus v22.XX atop DPDK v20.11 (-> See [control/dyso/pcpp](https://github.com/dyso-project/dyso_p4/tree/main/control/dyso/pcpp))


## Query generator
![QueryGen](/misc/querygen.jpg)


We generate queries via Tofino traffic generator. It generates 80 Mpps query stream with shifting popularity ranks (see the evaluation part of paper) in following steps:
1. From [misc/zipf.txt](https://github.com/dyso-project/dyso_p4/blob/main/misc/zipf.txt), the switch's register array (size 524288) reads the flowIDs (4 Bytes) and save them. The flowIDs are ranging from 1 to N (e.g., 138092), where N is the number of flows. Here, we use zipf(0.99) distribution.
2. There is an offset register X, by default 0. For every generated packet, it samples randomly from the above registers. Once it samples one register with flowID=Y, the packet's final key is X+Y (or X-Y).
3. To shift the popularity of content, we adjust the offset X. We periodically increase the offset value to generate dynamic workloads.

For simplicity, in this prototype, we generate key-matching queries (key size = 4 Bytes) with no values.

## Running DySO control loop

### P4 control plane
The file [control/dyso/cpp/dyso.cpp](https://github.com/dyso-project/dyso_p4/blob/main/control/dyso/cpp/dyso.cpp) is the runtime interface code written in C++. Note that it cannot be compiled after SDE-9.7.0 as we find some build library is missing. instead, you can compile via CMakefile that Tofino's providied examples are using. 

### P4 source code
P4 source code is in [p4src/dyso](https://github.com/dyso-project/dyso_p4/tree/main/p4src/dyso). Note that the key-matching and lookup-history storage codes are implemented in pipeline1, whereas pipeline0 performs query generator with key allocation.

### DySO's policy data structure
In folder [control/dyso/pcpp/src](https://github.com/dyso-project/dyso_p4/tree/main/control/dyso/pcpp/src), there are scripts implementing the policy data structure (see the paper) and other utility files such as Lock-free queue and efficient software hash table. 

### PcapPlusPlus source code
In folder [control/dyso/pcpp](https://github.com/dyso-project/dyso_p4/tree/main/control/dyso/pcpp), there are codes for PcapPlusPlus threading with DPDK custom packet header parsers. 

### Simulation instruction
See [dyso_simulator.py](dyso_simulation.py) for details.
