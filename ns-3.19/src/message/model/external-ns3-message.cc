#include "external-ns3-message.h"

namespace ns3 {

// definition of virtual destructor needed, otherwise the linker complains
ExternalNS3Message::~ExternalNS3Message() {
}


std::ostream & operator << (std::ostream &os, const ExternalNS3Message &msg)
{
  msg.Print (os);
  return os;
}

} // namespace ns3


