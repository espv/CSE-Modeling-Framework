/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef TELOSB_HELPER_H
#define TELOSB_HELPER_H

#include "ns3/telosb.h"

namespace ns3 {

/**
 * Helper for creating CC2420NetDevices (or CC2420InterfaceNetDevices,
 * respectively) and connecting them to a channel.
 */
    class TelosBHelper
    {
    public:
        TelosBHelper();

        /*
         * For installing the provided TelosB
         */
        TelosB *TelosBHelper::Install (NodeContainer nodes, std::string deviceFile)
    };
}

#endif /* TELOSB_HELPER_H */
