#ifndef ADDRESS_TAG_H_
#define ADDRESS_TAG_H_

#include "ns3/tag.h"
#include "ns3/address.h"

namespace ns3
{
/**
 * \ingroup packet
 *
 * \brief AddressTag is used to set source, dest and protocol number to a packet.
 *
 * AddressTag makes it possible to shorten the arguments of methods when packets are hand around.
 * It encapsulate the source & destination addresses and the protocol number of a packet.
 * It is used in CC2420NetDevice (methods: Receive & SendFrom) to hide these details for subjacent layers (PHY [+FSM], Channel).
 */
class AddressTag : public Tag
{
  public:
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize (void) const;
    virtual void Serialize (TagBuffer i) const;
    virtual void Deserialize (TagBuffer i);
    virtual void Print (std::ostream &os) const;

    // these are our accessors to our tag structure
    /**
     * \param source the source address
     * \param dest the destination address
     * \param protocolNumber the protocol number
     *
     * This method sets the values of the AddressTag.
     */
    void SetData (Address source,
                  Address dest,
                  uint16_t protocolNumber);
    /**
     * \param source the source address
     * \param dest the destination address
     * \param protocolNumber the protocol number
     *
     * This method sets the values of the AddressTag.
     */				  
    Address GetSource (void) const;
    Address GetDestination (void) const;
    uint16_t GetProtocolNumber (void) const;

  private:
	//helper methods for Serialize &  Deserialize
    void WriteAddressTo(TagBuffer &i, const Address &ad) const;
    Address ReadAddressFrom (TagBuffer &i);
    uint8_t getType(const Address &ad) const;

    Address m_source;
    Address m_dest;
    uint16_t m_protocolNumber;
};

//overwrite "<<"-operator to use the Print-Method, if a tag is concatenate to a output stream
std::ostream & operator << (std::ostream &os, const Tag &tag);

} /* namespace ns3 */
#endif /* ADDRESS_TAG_H_ */
