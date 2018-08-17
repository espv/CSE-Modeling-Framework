#include "cc2420-header.h"


namespace ns3
{
  NS_OBJECT_ENSURE_REGISTERED (CC2420Header);

  CC2420Header::CC2420Header ()
  {
    // we must provide a public default constructor,
    // implicit or explicit, but never private.
  }
  CC2420Header::~CC2420Header ()
  {
  }

  TypeId
  CC2420Header::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::CC2420Header")
      .SetParent<Header> ()
      .AddConstructor<CC2420Header> ()
    ;
    return tid;
  }
  TypeId
  CC2420Header::GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }

  void
  CC2420Header::Print (std::ostream &os) const
  {
    // This method is invoked by the packet printing
    // routines to print the content of my header.
    os << "Preamble_Length=" << (uint32_t)m_preambleLength <<
       " SyncWord=0x" << std::hex << m_syncWord << std::dec <<
       " Frame_Length=" << (uint32_t)m_frameLength;

  }

  uint32_t
  CC2420Header::GetSerializedSize (void) const
  {
    /* CC2420 Header: Synchronisation Header + Phy Header
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
    return 2+m_preambleLength+1+1;
  }
  void
  CC2420Header::Serialize (Buffer::Iterator start) const
  {
    if (m_preambleLength > 0){
        // write m_preambleLength in the first byte of the preamble
        start.WriteU8(m_preambleLength);
        // write the following [m_preambleLength-1] zero Byte
        start.WriteU8(0, m_preambleLength-1);
    }
    start.WriteU8(0); // two zero Symbols

    //split the SyncWord into the necessary 4 bit parts (SW0 SW1)
    uint8_t sw0sw1;
    sw0sw1 = ( ( ((m_syncWord & 0xf) == 0xf) ? 0 : (m_syncWord & 0xf) ) <<4 )
           + ( (((m_syncWord>>4) & 0xf) == 0xf) ? 0 : ((m_syncWord>>4) & 0xf) );
    start.WriteU8(sw0sw1); // write SW0 and SW1

    //split the SyncWord into the necessary 4 bit parts (SW2 SW3)
    uint8_t sw2sw3;
    sw2sw3 = ( (((m_syncWord>>8) & 0xf) == 0xf) ? 0 : ((m_syncWord>>8) & 0xf) <<4 )
           + ( (((m_syncWord>>12) & 0xf) == 0xf) ? 0 : ((m_syncWord>>12) & 0xf) );
    start.WriteU8(sw2sw3); // write SW2 and SW3

    //Write Frame_Length
    start.WriteU8(m_frameLength);

  }
  uint32_t
  CC2420Header::Deserialize (Buffer::Iterator start)
  {
    //read Preamble_Length form first Byte
    m_preambleLength = start.ReadU8();
    if (m_preambleLength > 0){
        //read the following zero Bytes of the preamble
        for (uint8_t i = 0; i < m_preambleLength-1; i++)
          {
            start.ReadU8 ();
          }
        //read the invariable part of the preamble
        start.ReadU8(); // read two zero Symbols
    }else{
        // we already read the invariable part of the preamble
        // so the following bytes is SW0+SW1
    }

    uint8_t sw0sw1 = start.ReadU8(); // read SW0 and SW1
    uint8_t sw2sw3 = start.ReadU8(); // read SW2 and SW3

    //patch the SyncWord together
    m_syncWord =  ((sw2sw3 & 0xf)<<12) + (((sw2sw3>>4) & 0xf)<<8) +
                  ((sw0sw1 & 0xf)<<4) + ((sw0sw1>>4) & 0xf);
    //read Frame_Length
    m_frameLength = start.ReadU8();

    // we return the number of bytes effectively read.
    return m_preambleLength // zero BYTES
           +1 // two zero SYMBOLS
           +1 // SW0 + SW1
           +1 // SW2 + SW3
           +1; // Frame_Length
  }

  void
  CC2420Header::SetData (uint8_t preambleLength,
                         uint16_t syncWord,
                         uint8_t frameLength)
  {
    m_preambleLength = preambleLength;
    m_syncWord = syncWord;
    m_frameLength = frameLength;
  }
  uint16_t
  CC2420Header::GetSyncWord (void) const
  {
    return m_syncWord;
  }
  uint8_t
  CC2420Header::GetFrameLength (void) const
  {
    return m_frameLength;
  }
  bool
  CC2420Header::CompareSyncWord(uint16_t otherSyncWord) const
  {
    //split the SyncWord into the necessary 4 bit parts
    uint8_t my_sw0 = ((m_syncWord & 0xf) == 0xf) ? 0 : (m_syncWord & 0xf);
    uint8_t my_sw1 = (((m_syncWord>>4) & 0xf) == 0xf) ? 0 : ((m_syncWord>>4) & 0xf);
    uint8_t my_sw2 = (((m_syncWord>>8) & 0xf) == 0xf) ? 0 : ((m_syncWord>>8) & 0xf);
    uint8_t my_sw3 = (((m_syncWord>>12) & 0xf) == 0xf) ? 0 : ((m_syncWord>>12) & 0xf);

    //split the other SyncWord into the necessary 4 bit parts
    uint8_t other_sw0 = ((otherSyncWord & 0xf) == 0xf) ? 0 : (otherSyncWord & 0xf);
    uint8_t other_sw1 = (((otherSyncWord>>4) & 0xf) == 0xf) ? 0 : ((otherSyncWord>>4) & 0xf);
    uint8_t other_sw2 = (((otherSyncWord>>8) & 0xf) == 0xf) ? 0 : ((otherSyncWord>>8) & 0xf);
    uint8_t other_sw3 = (((otherSyncWord>>12) & 0xf) == 0xf) ? 0 : ((otherSyncWord>>12) & 0xf);


    return my_sw0 == other_sw0 &&
           my_sw1 == other_sw1 &&
           my_sw2 == other_sw2 &&
           my_sw3 == other_sw3;
  }
} /* namespace ns3 */
