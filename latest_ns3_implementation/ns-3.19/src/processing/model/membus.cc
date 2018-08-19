#include "ns3/core-module.h"
#include "membus.h"
#include "program.h"

namespace ns3 {

TypeId
MemBus::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Processing::MemBus")
    .SetParent<Object> ()
    .AddConstructor<MemBus> ()
    .AddAttribute ("frequency", "The clock frequency of the CPU in the device",
                   UintegerValue (1000),
                   MakeUintegerAccessor (&MemBus::freq),
		   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

MemBus::MemBus()
{
}

MemBus::MemBus(int contentionModel)
{
  // TODO
}


// We want the consumption explicitly to
// save time iterating the resource vector.
void
MemBus::Contend(ProcessingInstance *pi)
{
  m_contenders.push_back(pi);
  pi->remaining[MEMSTALLCYCLES].amount = 
    freq / m_contenders.size() * (pi->remaining[MEMORYACCESSES].amount);
}

// NEXT: MemBus::UnContend

}
