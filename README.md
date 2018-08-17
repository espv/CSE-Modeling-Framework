# previous-master-thesis
Code of master thesis by Øystein Dale

To make a device model of TelosB that is realistic, we need to do two things:
1: Create a device file that has similar logic to the real device, and spends as long time processing it, and
2: use network models for sixlowpan and 802.15.4 to get accurate simulation of the parts that we don't trace.

Step 2 is very important if we want to compare the device model with the execution of a real device.
What we've noticed is that we can trace a real device to spend approximately 10 ms on each packet.
However, the mote can only forward packets that are sent every 17 ms. Any lower than that and the
device starts dropping packets. We also conducted a small experiment to rule out that the network
got congested because the nodes were too close to each other, but we found that that was not the case.
The conclusion then is that step 2 is important and that we should use the sixlowpan model provided
by NS3 at https://www.nsnam.org/doxygen/example-sixlowpan_8cc_source.html to realistically model the
network communication of the device, and not just the intra-OS delay.
