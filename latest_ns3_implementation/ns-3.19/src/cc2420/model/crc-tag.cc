#include "crc-tag.h"

namespace ns3
{

  TypeId
  CrcTag::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::CrcTag")
      .SetParent<Tag> ()
      .AddConstructor<CrcTag> ()
    ;
    return tid;
  }
  TypeId
  CrcTag::GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }
  uint32_t
  CrcTag::GetSerializedSize (void) const
  {
    return 0;
  }
  void
  CrcTag::Serialize (TagBuffer i) const
  {
  }
  void
  CrcTag::Deserialize (TagBuffer i)
  {
  }

  void
  CrcTag::Print (std::ostream &os) const
  {
    os << "Checksum-Trailer appended";
  }
} /* namespace ns3 */
