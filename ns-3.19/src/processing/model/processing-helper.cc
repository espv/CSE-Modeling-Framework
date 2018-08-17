#include "hwmodel.h"
#include "processing-helper.h"
#include "program.h"
#include "processing.h"
#include "peu.h"
#include "membus.h"
#include "interrupt-controller.h"
#include <ns3/drop-tail-queue.h>

// The following are included for the parser
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace ns3 {

TypeId
ProcessingHelper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ProcessingHelper")
    .SetParent<Object> ()
    .AddConstructor<ProcessingHelper> ()
    .AddAttribute ("cpuFrequency", "The clock frequency of the CPU",
                   UintegerValue (1000),
                   MakeUintegerAccessor (&ProcessingHelper::m_cpuFreq),
		   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("memFrequency", "The clock frequency of the memory bus",
                   UintegerValue (100),
                   MakeUintegerAccessor (&ProcessingHelper::m_memFreq),
		   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("cacheLineSize", "The size of the cache lines of the memory cache",
                   UintegerValue (64),
                   MakeUintegerAccessor (&ProcessingHelper::m_cacheLineSize),
		   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("tracingOverhead", "The average number of cycles spent for each trace call",
                   UintegerValue (100),
                   MakeUintegerAccessor (&ProcessingHelper::m_traceOverhead),
		   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

ProcessingHelper::ProcessingHelper() {
}

void
ProcessingHelper::Install(std::string device, Ptr<Node> n)
{
  Install(device, NodeContainer(n));
}

void
ProcessingHelper::Install(std::string device, NodeContainer nc)
{
  // Traverse all nodes in the container
  uint32_t nNodes = nc.GetN ();
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<Node> p = nc.Get (i);

      // Create processing object for this node
      Ptr<Processing> newProc = CreateObject<Processing> ();

      newProc->Initialize(device);
    }
}

}
