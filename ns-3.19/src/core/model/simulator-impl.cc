#include "simulator-impl.h"
#include "log.h"

NS_LOG_COMPONENT_DEFINE ("SimulatorImpl");

namespace ns3 {

TypeId 
SimulatorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SimulatorImpl")
    .SetParent<Object> ()
  ;
  return tid;
}

/* STEIN: avoid this as pure virtual function to avoid
   requiring all scheduler implementations to implement
   this function */
Time SimulatorImpl::Next (void) {
  return Time(0);
}
/* STEIN */

} // namespace ns3
