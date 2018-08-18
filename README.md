# TelosB CSW model

This project consists of ns-3.19 plus an extension that enables accurate simulation of the CSW of devices in the network. An example of such a device is a TelosB mote that executes the TinyOS operating system. Without the CSW model, only transmission delay is simulated when a Mote A sends a packet to Mote C via B. In reality, Mote B spends time processing the packets it receives from Mote A before transmitting them to Mote C. That subset of the end-to-end delay is called processing delay. The CSW model simulates a subset of the processing delay called intra-OS delay, which describes the time it takes for the OS to process incoming packets.

The code in ns-3.19/source/processing contains the execution environment and existing models for introducing processing delays. Stein Kristiansen authored the logic of the execution environment for his Ph.D. thesis named "A Methodology to Model the Execution of Communication Software for Accurate Simulation of Distributed Systems". Further publications in https://dl.acm.org/citation.cfm?id=2746233 and https://dl.acm.org/citation.cfm?id=2486102 describe the methodology in detail. Øystein Dale describes in his Master thesis named "Modeling, analysis, and simulation of communication software execution on multicore devices" is about the creation of a CSW model of a multi-core device. My master thesis named "Communication software model of WSN device for more accurate simulation in ns-3" is about the creation of a CSW model of a resource-constrained device used in WSNs.

How to run in terminal window from git root directory:<br/>
1: $```cd ns-3.19```<br/>
2: $```waf_configure_scripts/configure_optimized_waf.sh # If you are not going to debug. Otherwise, run configure_debug_waf.sh```<br/>
3: $```./waf```<br/>
    ```# If you want to run with gdb, run run_gdb_experiment.sh; if you want to run with valgrind, run run_valgrind_experiment.sh```<br/>
4: $```./csw_model_execution_scripts/run_experiment.sh telosb```

This repo also has a submodule called tinyos-instrumented-for-telosb that contains the instrumented TinyOS code used to trace the temporal behavior of the CSW of TinyOS.
Steps to compile TinyOS: (requires a working java installation >= java 5, installation of the tinyos-tools and nescc packages)
1: cd tinyos-instrumented-for-telosb/tinyos-main/tos
2: ./Bootstrap
3: ./configure
4: make
5: make install

The ForwardPacketsAndTraceCSW application has been used to trace the temporal behavior of the CSW of TinyOS while doing packet forwarding. You can test the app out on a real topology with three motes or by using COOJA (COntiki Os JAva simulator). The behavior of each node depends on their node ID that is automatically assigned in COOJA and explicitly assigned when compiling for real motes.

