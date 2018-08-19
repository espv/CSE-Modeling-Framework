#include "cripple-tag.h"

namespace ns3
{

  TypeId
  CrippleTag::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::CrippleTag")
      .SetParent<Tag> ()
      .AddConstructor<CrippleTag> ()
    ;
    return tid;
  }
  TypeId
  CrippleTag::GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }
  uint32_t
  CrippleTag::GetSerializedSize (void) const
  {
    return 0;
  }
  void
  CrippleTag::Serialize (TagBuffer i) const
  {
  }
  void
  CrippleTag::Deserialize (TagBuffer i)
  {
  }

  void
  CrippleTag::Print (std::ostream &os) const
  {
    os << "Packet flagged as crippled.";
  }
} /* namespace ns3 */
