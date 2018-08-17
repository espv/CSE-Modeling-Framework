# TelosB CSW model

This project consists of ns-3.19 plus an extension that enables accurate simulation of the CSW of devices in the network. An example of such a device is a TelosB mote that executes the TinyOS operating system. Without the CSW model, only transmission delay is simulated when a Mote A sends a packet to Mote C via B. In reality, Mote B spends time processing the packets it receives from Mote A before transmitting them to Mote C. That subset of the end-to-end delay is called processing delay. The CSW model simulates a subset of the processing delay called intra-OS delay, which describes the time it takes for the OS to process incoming packets.

How to run in terminal window from git root directory:
1: $```cd ns-3.19```
2: $```waf_configure_scripts/configure_optimized_waf.sh # If you are not going to debug. Otherwise, run configure_debug_waf.sh```
3: $```./waf```
    ```# If you want to run with gdb, run run_gdb_experiment.sh; if you want to run with valgrind, run run_valgrind_experiment.sh```
4: $```./csw_model_execution_scripts/run_experiment.sh telosb```

