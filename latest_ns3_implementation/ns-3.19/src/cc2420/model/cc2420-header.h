#ifndef CC2420_HEADER_H_
#define CC2420_HEADER_H_

#include "ns3/header.h"

namespace ns3
{

  /*
   *\brief A Header containing the 802.15.4 Synchronisation & PHY Header
   * 
   * A CC2420 PHY Header implementation containing preamble, syncword and frame length.
   * It serialize the data almost(1) as a real packet send from the PHY-Layer,
   * so look out for some byte twists.
   *
   * \attention (1) The first byte of the preamble is used to store the length of the preamble.
   * \attention This information is neccessary to deserialize the header correct.
   *
   * CC2420 Header: Synchronisation Header + Phy Header
   *  Synchronisation Header: Preamble + SFD
   *    Preamble: (2*PL) zero symbols + 2 zero symbols + (SW0 + SW1)(2+PL Byte)
   *    SFD: SW2 + SW3                                              (   1 Byte)
   *  Phy Header: Frame Length (0-127)                              (   1 Byte)
   *
   * with:
   *      1 symbol = 4 bit
   *      2 symbols = 1 Byte
   *      SW0 = SYNCWORD[3:0] (4 bit)
   *      SW1 = SYNCWORD[7:4] (4 bit)
   *      SW2 = SYNCWORD[11:8] (4 bit)
   *      SW3 = SYNCWORD[15:12] (4 bit)
   *      PL: PREAMBLE_LENGTH: 0-15
   */
  class CC2420Header : public Header
  {
  public:

    CC2420Header ();
    virtual ~CC2420Header ();

    // allow protocol-specific access to the header data.
    /**
     * \param preambleLength - the size of the preamble in bytes
     * \param syncword - the 2 byte SyncWord
     * \param frameLength - the length of the whole frame (including possible checksum)
     *
     * Set the corresponding values.
     */
    void SetData (uint8_t preambleLength, uint16_t syncword, uint8_t frameLength);
    uint16_t GetSyncWord (void) const;
    uint8_t GetFrameLength (void) const;

    /**
     * \param otherSyncWord - to compare to
     * \return true, if both Syncwords are identical
     *
     * \brief Compare the own SyncWord of the header to the other (argument).
     * \attention The compare treats 0xf as 0 (according to the CC2420-DataSheet).
     */
    bool CompareSyncWord(uint16_t otherSyncWord) const;

    // must be implemented to become a valid new header.
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual void Print (std::ostream &os) const;

    /**
     * This method is called when the header is added to a packet. It
     * replaces f symbols in the sync word by 0 symbols, since this is also
     * done by the real hardware.
     */
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual uint32_t GetSerializedSize (void) const;
  private:
    uint8_t m_preambleLength;
    uint16_t m_syncWord;
    uint8_t m_frameLength;
  };

} /* namespace ns3 */
#endif /* CC2420_HEADER_H_ */
