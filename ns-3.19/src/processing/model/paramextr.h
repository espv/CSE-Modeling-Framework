#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/empty.h"
#include "ns3/event-impl.h"
#include "ns3/random-variable.h"
#include "ns3/callback.h"
#include "ns3/simulator.h"

/* TODO:
   - Should support dynamic insertion of functions related to
     string identifiers. Packet size is there by default (added
     in the constructor).
   - Should distinguish between protocol and packet extractors.
   - Thus, one should be able to write e.g.,
     PacketExtractor(packet, "entropy") or
     ProtocolExtractor("ip::routingtablesize"). These should then
     call the functions registered in the extractor map, that
     was dynamically stored by e.g., the constructors of the
     protocol. I.e., in the same way the services was stored!
*/

class ParameterExtractor : SimpleRefCount<ParameterExtractor>
{
 public:
  static uint32_t PacketSize(Ptr<Packet> p);
};
