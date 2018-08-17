#ifndef CC2420_TRAILER_H_
#define CC2420_TRAILER_H_

#include "ns3/trailer.h"
#include "ns3/packet.h"

namespace ns3
{

  /*
   *\brief A Trailer containing the 802.15.4 FCS.
   * 
   * A CC2420 PHY Trailer implementation containing the Frame Check Sequnce(FCS, CRC checksum).
   * It does the checksum calculation is itself, simply pass the packet to CalcFcs.
   * 
   * /attention The algorithm used is CRC-CCITT XModem (CRC16C) with no start value and the polynom 0x1021.
   * /attention This is the correct algorithm according to the 802.15.4 specification.
   * /attention It is often confused with other CRC algorithms, so with the start value & polynom it can be correctly identified.
   */
  class CC2420Trailer : public Trailer
  {
  public:

    CC2420Trailer ();
    virtual ~CC2420Trailer ();

    /**
     * \brief Updates the Fcs Field to the correct FCS
     * \param p Reference to a packet on which the FCS should be
     * calculated. The packet must not currently contain an
     * CC2420Trailer.
     */
    void CalcFcs (Ptr<const Packet> p);

    /**
     * \brief Sets the FCS to a new value
     * \param fcs New FCS value
     */
    void SetFcs (uint16_t fcs);

    /**
     * \return the FCS contained in this trailer
     */
    uint16_t GetFcs ();

    /**
     * Calculate an FCS on the provided packet and check this value against
     * the FCS found when the trailer was deserialized (the one in the transmitted
     * packet).
     *
     * If FCS checking is disabled, this method will always
     * return true.
     *
     * \param p Reference to the packet on which the FCS should be
     * calculated. The packet should not contain an EthernetTrailer.
     *
     * \return Returns true if the Packet FCS matches the FCS in the trailer,
     * false otherwise.
     */
    bool CheckFcs (Ptr<const Packet> p) const;

    // must be implemented to become a valid new header.
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual void Print (std::ostream &os) const;
    virtual void Serialize (Buffer::Iterator end) const;
    virtual uint32_t Deserialize (Buffer::Iterator end);
    virtual uint32_t GetSerializedSize (void) const;

  private:
    //initialize  the tables with values for the crc algorithm
    static void init_crcccitt_tab( void );
    uint16_t update_crc_ccitt(uint16_t crc, uint8_t c) const;

    uint16_t DoCalcFcs (uint8_t const *buffer, size_t len) const;

    static bool crc_tabccitt_init; //initialize as false
    static uint16_t crc_tabccitt[256];

    uint16_t m_fcs; /// Value of the fcs contained in the trailer
  };

} /* namespace ns3 */
#endif /* CC2420_TRAILER_H_ */
