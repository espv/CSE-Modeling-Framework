#ifndef CC2420PHY_H_
#define CC2420PHY_H_

#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"

#include "cc2420-channel.h"
#include "cc2420-fsm.h"
#include "cc2420-net-device.h"


namespace ns3
{
  class CC2420Phy : public ns3::Object
  {
  public:
    CC2420Phy();
    virtual
    ~CC2420Phy();

    static TypeId GetTypeId (void);

    // *** Receive Packet from Channel
    void ForwardToDevice (Ptr<Packet> packet, double rxPowerDbm);
    void Receive (Ptr<Packet> packet, double rxPowerDbm);

    // *** Handle transmission requests from NetDevice
    bool Send (Ptr<Packet> packet, bool checkCCA); //nutzt allocDownPacket
    void SendToChannel(Ptr<Packet> packet, double txPowerDbm);

    void SetChannel (Ptr<CC2420Channel> channel);
    Ptr<CC2420Channel> GetChannel (void);

    void ChangeChannelNumber (uint8_t channelnumber);

    uint8_t GetChannelNumber ();

    void SetDevice (Ptr<CC2420NetDevice> device);
    Ptr<CC2420NetDevice> GetDevice (void) const;

    uint32_t GetTransmissionRateBytesPerSecond();
    int32_t GetCaptureThresholdDb();

    /**
     * Returns the value for the CCA threshold. The internal transceiver offset of
     * -45 is subtracted, i.e. the actual CCA threshold is the return value minus 45.
     */
    int32_t GetCSThreshold();

    /**
     * Returns the value for the CCA threshold. The internal transceiver offset of
     * -45 is not subtracted, i.e. the return value is the actual CCA threshold in dBm.
     */
    int32_t GetCSThresholdDBm();

    // *** Configure CCA delay
    Time getMinCCADelay();
    Time getMaxCCADelay();

    void SetPowerLevel(uint8_t txPowerLevel);
    uint8_t GetPowerLevel();
    double GetPowerdBm();

    uint16_t GetMaxPayloadSize();


    // needed for the CC2420InterfaceNetDevice
    uint8_t GetCCAMode();
    uint8_t GetCCAHyst();
    bool GetTxTurnaround();
    bool GetAutoCrc();
    uint8_t GetPreambleLength();
    uint16_t GetSyncWord();

    void SetCCAMode(uint8_t ccaMode);
    void SetCCAHyst(uint8_t ccaHyst);
    void SetTxTurnaround(bool txTurnaround);
    void SetAutoCrc(bool autoCrc);
    void SetPreambleLength(uint8_t preambleLength);
    void SetSyncWord(uint16_t syncWord);

    /**
     * Sets the value for the CCA threshold. The parameter is the same value
     * as for the transceiver, the internal offset of -45 is added automatically.
     */
    void SetCSThreshold(int8_t ccaThr);

  private:
    void calcPower();
   
    // *** Configure CCA delay
    Time m_MinCCADelay;
    Time m_MaxCCADelay;
    uint8_t m_txPowerLevel;
    double m_txPowerDbm;
    uint8_t m_preambleLength;
    bool m_autoCrc;
    bool m_txTurnAround;
    uint8_t m_channelNumber;
    uint8_t m_CCA_HYST;
    uint8_t m_CCA_Mode;
    uint16_t m_syncWord;
    //RX Threshold
    int32_t m_rxThreshold_dBm; //-92dBm
    int32_t m_captureThreshold_dB;// 10dB // capture Threshold of 10dB
    //carrier sense threshold (dBm)
    int32_t m_CCA_THR; // -77dBm //https://www.nsnam.org/bugzilla/show_bug.cgi?id=689#c9
    // value is stored WITH the internal offset of the transceiver

    uint16_t m_MaxPayloadSize; // the maximal payload of transmitted packets

    Ptr<CC2420FSM> m_fsm;
    Ptr<CC2420Channel> m_channel;
    Ptr<CC2420NetDevice> m_device;

    static const uint32_t m_TransmissionRate_bits_per_second = 250000; //bits per second
    static const uint32_t m_TransmissionRate_Bytes_per_second = m_TransmissionRate_bits_per_second/8; //Bytes per second

  };
} /* namespace ns3 */
#endif /* CC2420PHY_H_ */
