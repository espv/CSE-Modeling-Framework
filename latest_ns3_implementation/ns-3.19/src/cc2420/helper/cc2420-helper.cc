#include "ns3/propagation-module.h"
#include "cc2420-helper.h"
#include "ns3/cc2420-interface-net-device.h"

namespace ns3 {

  CC2420Helper::CC2420Helper()
  {
  }

  NetDeviceContainer
  CC2420Helper::Install (NodeContainer nodes, Ptr<CC2420Channel> channel, bool interfaceDev)
  {
    NetDeviceContainer devices;
    for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
      {
        Ptr<CC2420NetDevice> dev;
        if(interfaceDev)
        	dev = CreateObject<CC2420InterfaceNetDevice>();
        else
        	dev = CreateObject<CC2420NetDevice>();

        dev->SetAddress (Mac48Address::Allocate ());
        dev->SetChannel (channel);
        (*i)->AddDevice (dev);
        devices.Add (dev);
      }
    return devices;
  }

  NetDeviceContainer
  CC2420Helper::Install (NodeContainer nodes, bool interfaceDev)
  {
    Ptr<CC2420Channel> channel = CreateObject<CC2420Channel> ();
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    channel->SetPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
    return Install (nodes, channel, interfaceDev);
  }

}

