#ifndef __CC2420_HELPER_H__
#define __CC2420_HELPER_H__

#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/cc2420-channel.h"

namespace ns3 {

/**
 * Helper for creating CC2420NetDevices (or CC2420InterfaceNetDevices,
 * respectively) and connecting them to a channel.
 */
  class CC2420Helper
  {
  public:
	CC2420Helper();

	/**
	 * Creates a CC2420NetDevice for each node in <code>nodes</code> an assigns
	 * it to <code>channel</code> (the phy layer is created when calling the
	 * constructor for CC2420NetDevice). The created NetDevices are added to a
	 * NetDeviceContainer, which is returned.
	 * If <code>interfaceDev==true</code>, CC2420InterfaceNetDevices are
	 * created instead. This is a subclass of CC2420NetDevice for use with
	 * the simulation framework.
	 */
    NetDeviceContainer Install (NodeContainer nodes, Ptr<CC2420Channel> channel, bool interfaceDev);

    /**
     * Creates a CC2420Channel and calls Install(nodes, channel, interfaceDev).
     */
    NetDeviceContainer Install (NodeContainer nodes, bool interfaceDev);
  };
}

#endif /* __CC2420_HELPER_H__ */

