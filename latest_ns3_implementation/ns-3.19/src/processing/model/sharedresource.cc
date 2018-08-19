#include "sharedresource.h"

namespace ns3 {

SharedResource::SharedResource ()
  : m_freq(0)
{
}

SharedResource::SharedResource(int freq)
  : m_freq(freq)
{
}

SharedResource::~SharedResource ()
{
}

int
SharedResource::Consume(int units)
{
  // Return duration according to contention model
  return 0;
}

}
