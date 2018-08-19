#include "execenv.h"
#include "program.h"
#include "membus.h"
#include "hwmodel.h"
#include "interrupt-controller.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HWModel");
NS_OBJECT_ENSURE_REGISTERED (HWModel);

TypeId 
HWModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::processing::HWModel")
    .SetParent<Object> ()
    .AddConstructor<HWModel> ();

  return tid;
}

HWModel::~HWModel()
{
}

HWModel::HWModel()
{
  // Create CPU
//  cpu = CreateObject<CPU> ();
//  cpu->hwModel = this;

  // Create memory bus
  m_memBus = CreateObject<MemBus> ();
}

Ptr<CPU> HWModel::AddCPU(Ptr<CPU> newCPU) {
    cpus.push_back(newCPU);
    return newCPU;
}

} // namespace ns3
