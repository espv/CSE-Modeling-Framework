#include "paramextr.h"

static uint32_t ParameterExtractor::PacketSize(Ptr<Packet> p)
{
  return p.GetSize();
}
