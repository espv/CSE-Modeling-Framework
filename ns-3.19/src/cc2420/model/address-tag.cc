#include "address-tag.h"

namespace ns3
{

  TypeId
  AddressTag::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::AddressTag")
      .SetParent<Tag> ()
      .AddConstructor<AddressTag> ()
    ;
    return tid;
  }
  TypeId
  AddressTag::GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }
  uint32_t
  AddressTag::GetSerializedSize (void) const
  {
    return 2+m_source.GetLength()+
           2+m_dest.GetLength()+
           2;
  }
  void
  AddressTag::Serialize (TagBuffer i) const
  {
    WriteAddressTo(i, m_source);
    WriteAddressTo(i, m_dest);
    i.WriteU16(m_protocolNumber);

  }
  void
  AddressTag::Deserialize (TagBuffer i)
  {
    m_source = ReadAddressFrom(i);
    m_dest = ReadAddressFrom(i);
    m_protocolNumber = i.ReadU16();
  }

  void
  AddressTag::Print (std::ostream &os) const
  {
    os << "Source=" << m_source << ", Destination=" << m_dest
       << ", ProtocolNumber=" << (uint32_t)(m_protocolNumber);
  }

  void
  AddressTag::SetData(Address source, Address dest,
      uint16_t protocolNumber)
  {
    m_source = source;
    m_dest = dest;
    m_protocolNumber = protocolNumber;
  }

  Address
  AddressTag::GetSource(void) const
  {
    return m_source;
  }

  Address
  AddressTag::GetDestination(void) const
  {
    return m_dest;
  }

  uint16_t
  AddressTag::GetProtocolNumber(void) const
  {
    return m_protocolNumber;
  }

  void
  AddressTag::WriteAddressTo(TagBuffer& i, const Address& ad) const
  {
    i.WriteU8(getType(ad));
    i.WriteU8 (ad.GetLength());
    uint8_t byteArray[ad.GetLength()];
    ad.CopyTo(byteArray);
    i.Write(byteArray, ad.GetLength());
  }

  Address AddressTag::ReadAddressFrom (TagBuffer &i)
  {
    uint8_t type = i.ReadU8();
    uint8_t len = i.ReadU8();
    uint8_t byteArray[len];
    i.Read (byteArray, len);

    return Address(type, byteArray, len);
  }

  uint8_t AddressTag::getType(const Address &ad) const
  {
    for(uint8_t type=0; type<=255; type++){
        if(ad.IsMatchingType(type)){
            return type;
        }
    }
    return 0;
  }

  std::ostream & operator << (std::ostream &os, const Tag &tag)
  {
    tag.Print (os);
    return os;
  }
} /* namespace ns3 */
