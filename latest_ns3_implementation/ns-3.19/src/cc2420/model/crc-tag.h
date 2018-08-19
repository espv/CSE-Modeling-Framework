#ifndef CRC_TAG_H_
#define CRC_TAG_H_

#include "ns3/tag.h"

namespace ns3
{
/**
 * \ingroup packet
 *
 * \brief CrcTag is used to flag a packet, if there is a CrcTrailer.
 *
 * A packet can be searched for a tag, but not for a header or trailer,
 * so it must be previously known wether the packet has a particular Header/trailer.
 * So if a packet has a CrcTag, there is also a CC2420Trailer (containing a crc checksum).
 * The CrcTag doesn't contain any data and is only used as a flag in CC2420Phy.
 */
class CrcTag : public Tag
{
  public:
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;
};
//overwrite "<<"-operator to use the Print-Method, if a tag is concatenate to a output stream
std::ostream & operator << (std::ostream &os, const Tag &tag);

} /* namespace ns3 */
#endif /* CRC_TAG_H_ */
