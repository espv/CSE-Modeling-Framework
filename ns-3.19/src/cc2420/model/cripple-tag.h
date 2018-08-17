#ifndef CRIPPLE_TAG_H_
#define CRIPPLE_TAG_H_

#include "ns3/tag.h"

namespace ns3
{
/**
 * \ingroup packet
 *
 * \brief CrippleTag is used to flag a packet as crippled/noise.
 *
 * If a packet has a CrippleTag it is be treated as mutilated packet or nameless noise.
 * The CrcTag doesn't contain any data and is only used as a flag in CC2420Phy [+FSM] & CC2420Channel.
 * In CC2420Channel it is set to packet, if receiver could not listen to a packet in a whole.
 * In CC2420Phy it is additional set to packet with a different SyncWord, so the FSM treats it like noise for CCA.
 */
class CrippleTag : public Tag
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
#endif /* CRIPPLE_TAG_H_ */
