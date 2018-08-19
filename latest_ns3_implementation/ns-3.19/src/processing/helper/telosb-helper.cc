
#include "telosb-helper.h"
#include "../../network/utils/inet-socket-address.h"

namespace ns3 {

    TelosBHelper::TelosBHelper()
    {
    }

    TelosB *
    TelosBHelper::Install (NodeContainer nodes, std::string deviceFile)
    {
      TelosB *devices = new TelosB[nodes.GetN()];
      int n = 0;
      Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
              "cacheLineSize", UintegerValue(64), "tracingOverhead",
              UintegerValue(0));
      for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
      {
        eeh->Install(deviceFile, i);
        TelosB *mote = new TelosB(i);
        devices[n++] = mote;
      }
      return devices;
    }

    TelosB *
    TelosBHelper::Install (NodeContainer nodes, std::string deviceFile)
    {
      NetDeviceContainer devices;
      devices = cc2420.Install(nodes, true); // regular CC2420NetDevice

      InternetStackHelper stack;
      stack.Install(nodes);

      MobilityHelper mobility;

      // The way we want to configure this: mote 1 receives the packet from mote 2, but mote 3 does not receive it.
      // Mote 3 receives the packet from mote 2.
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (30.0),
                                     "DeltaY", DoubleValue (10.0),
                                     "GridWidth", UintegerValue (3),
                                     "LayoutType", StringValue ("RowFirst"));

      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (nodes);

      Ipv4AddressHelper address;
      address.SetBase("10.1.1.0", "255.255.255.0");

      Ipv4InterfaceContainer interfaces = address.Assign(devices);

      Ptr<CC2420InterfaceNetDevice> netDevice1 = nodes.Get(0)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
      Ptr<CC2420InterfaceNetDevice> netDevice2 = nodes.Get(1)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
      Ptr<CC2420InterfaceNetDevice> netDevice3 = nodes.Get(2)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
      TelosB *mote1 = new TelosB(nodes.Get(0), InetSocketAddress(interfaces.GetAddress(0), 9), netDevice1);
      TelosB *mote2 = new TelosB(nodes.Get(1), InetSocketAddress(interfaces.GetAddress(1), 9),
                                 InetSocketAddress(interfaces.GetAddress(2), 9), netDevice2);
      TelosB *mote3 = new TelosB(nodes.Get(2), InetSocketAddress(interfaces.GetAddress(2), 9), netDevice3);

      TelosB *devices = new TelosB[nodes.GetN()];
      int n = 0;
      Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
              "cacheLineSize", UintegerValue(64), "tracingOverhead",
              UintegerValue(0));
      for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
      {
        eeh->Install(deviceFile, i);
        TelosB *mote = new TelosB(i, InetSocketAddress(interfaces.GetAddress(n), 9), netDevices.Get(n));
        devices[n++] = mote;
      }
      return devices;
    }
}

