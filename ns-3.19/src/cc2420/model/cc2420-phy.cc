#include "cc2420-phy.h"
#include "cc2420-trailer.h"
#include "cc2420-header.h"
#include "crc-tag.h"
#include "cripple-tag.h"
#include "ns3/assert.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("CC2420Phy");

namespace ns3
{

  CC2420Phy::CC2420Phy()
  {
    m_txPowerLevel = 31; // default value from data sheet
    calcPower(); // calculate accordant tx power (in Dbm)

    // default value from data sheet (actually, the preamble length is
    // m_preambleLength+1 zero bytes, and a further zero byte is included in
    // the first of the two sync word bytes)
    m_preambleLength=2;

    m_autoCrc = true; // set by default, due to data sheet
    m_txTurnAround = true; // long turnaround by default, due to data sheet

    // default channel due to data sheet is 11; since channel number 11 is mapped
    // to index 0 (and so on), we have to subtract 11
    m_channelNumber = 0;
    m_CCA_HYST = 2; // default value from data sheet
    m_CCA_Mode = 3; // default value from data sheet
    m_syncWord = 0xA70F; // default value from data sheet

    SetCSThreshold(-32); // the internal transceiver offset of -45 is added automatically

    // these values are only set initially and are not changed afterwards
    //m_rxThreshold_dBm = -70.0;
    m_rxThreshold_dBm = -92; // value taken from experiments
    m_captureThreshold_dB = 10;

    m_MinCCADelay = MicroSeconds(0);
    m_MaxCCADelay = MicroSeconds(128); // 8 symbol periods

    // max payload in bytes (max size 127; -2 FCS(CRC); the rest of the MAC frame is not simulated)
    m_MaxPayloadSize = 125;

    m_fsm = CreateObject<CC2420FSM>();
    m_fsm->SetPhy(this);
  }

  CC2420Phy::~CC2420Phy()
  {
    m_fsm = 0;
    m_channel = 0;
  }

  TypeId
  CC2420Phy::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::CC2420Phy")
                              .SetParent<Object> ()
                              .AddConstructor<CC2420Phy> ()
                              ;
    return tid;
  }

  void CC2420Phy::ForwardToDevice (Ptr<Packet> packet, double rxPowerDbm){
    //Remove PHY-Header
    CC2420Header hdr;
    packet->RemoveHeader(hdr);

    bool crc=true;
    //check if Checksum-Trailer is set
    CrcTag crctag;
    if(packet->RemovePacketTag(crctag)){
      //remove Checksum and test against it
      CC2420Trailer tail;
      packet->RemoveTrailer(tail);
      crc = tail.CheckFcs(packet);
    }
    m_device->Receive(packet, crc, rxPowerDbm);
  }

  void CC2420Phy::Receive(Ptr<Packet> packet, double rxPowerDbm){

    if (m_CCA_Mode==1 && rxPowerDbm < m_CCA_THR-m_CCA_HYST){
        //rxPower below Carrier Sense Threshold
        //do not receive packet
//    	std::cout << "packet not received" << std::endl;
    }else{
      CC2420Header hdr;
      packet->PeekHeader(hdr);
      
      //check if packet is NOT crippled AND check for same syncWord 
      CrippleTag cripple;
      if (!packet->RemovePacketTag(cripple) &&
          hdr.CompareSyncWord(m_syncWord)){
        //identical SyncWord: receive

        //check rxPower
        if (rxPowerDbm<m_rxThreshold_dBm){
            //rxPower below Threshold -> Packet cannot successful decoded
            // Set error flag of the packet
           std::list < uint32_t > errorList =
               GetDevice()->GetPacketErrorList();
           errorList.push_back(packet->GetUid());
           GetDevice()->SetPacketErrorList(errorList);
        }
        m_fsm->rxFromMedium(packet, rxPowerDbm);
      }else{
        //crippled OR other SyncWord 
        
        //signal as Noise to FSM
        //use the CrippleTag as Noise Flag
        //cripple the packet/Flag as Noise
        packet->AddPacketTag(cripple);
        //send to FSM
        m_fsm->rxFromMedium(packet, rxPowerDbm);
      }
    }
  }

  bool CC2420Phy::Send (Ptr<Packet> packet, bool checkCCA){
    
    if(packet->GetSize() > m_MaxPayloadSize){
        m_device->NotifyTxFailure("Packet to long");
        return false;
    }else{
      if(m_autoCrc){
          CC2420Trailer tail;
          tail.CalcFcs(packet);
          packet->AddTrailer(tail);
          CrcTag crctag;
          packet->AddPacketTag(crctag);
      }
      CC2420Header hdr;
      hdr.SetData(m_preambleLength,m_syncWord,packet->GetSize());
      packet->AddHeader(hdr);
      return m_fsm->txRequest (packet, m_txPowerDbm, checkCCA);
    }
  }

  void CC2420Phy::SendToChannel(Ptr<Packet> packet, double txPowerDbm){
    Time transmission_time = Seconds(packet->GetSize()/m_TransmissionRate_Bytes_per_second);
    m_channel->Send(this, packet, txPowerDbm, transmission_time);
  }

  uint32_t CC2420Phy::GetTransmissionRateBytesPerSecond()
  {
    return m_TransmissionRate_Bytes_per_second;
  }

  int32_t CC2420Phy::GetCaptureThresholdDb()
  {
    return m_captureThreshold_dB;
  }

  int32_t CC2420Phy::GetCSThreshold()
  {
	// -45 is the internal offset of the transceiver
    return m_CCA_THR + 45;
  }

  int32_t CC2420Phy::GetCSThresholdDBm()
  {
    return m_CCA_THR;
  }


  // *** Configure CCA delay
  Time CC2420Phy::getMinCCADelay()
  {
    return m_MinCCADelay;
  }


  Time CC2420Phy::getMaxCCADelay(){
    return m_MaxCCADelay;
  }

  void CC2420Phy::SetDevice (Ptr<CC2420NetDevice> device)
  {
    m_device = device;
  }
  Ptr<CC2420NetDevice> CC2420Phy::GetDevice (void) const
  {
    return m_device;
  }

  void CC2420Phy::SetChannel (Ptr<CC2420Channel> channel){
    m_channel = channel;
    m_channel->Add(this);
  }

  void CC2420Phy::ChangeChannelNumber(uint8_t channelNumber){
    NS_ASSERT_MSG(channelNumber>=11 && channelNumber<=26, "Channel Number out of bound (11-26)");

    //set Channel Number 11 to Array-Index 0 (and Number 26 to Index 15)
    channelNumber = channelNumber-11;
    if(m_channelNumber == channelNumber){
        //no change
        //nothing to do
    }else{
        m_channelNumber = channelNumber;
        //Reset FSM according to Change of Channel Number
        m_fsm->reset(true);
        //Notify Channel, so it send us the messages of the new SubChannel
        m_channel->NotifyChannelNumberChange(this, m_channelNumber);
    }
  }

  uint8_t CC2420Phy::GetChannelNumber(){
	// since channel number 11 is mapped to index 0 (and so on), we have to add 11
    return m_channelNumber + 11;
  }

  Ptr<CC2420Channel> CC2420Phy::GetChannel ()
  {
    return m_channel;
  }

  void CC2420Phy::SetPowerLevel(uint8_t txPowerLevel)
  {
	  NS_ASSERT_MSG(txPowerLevel>=0 && txPowerLevel<=31,
			  "Power level can only have values between 0 and 31!");
	  m_txPowerLevel = txPowerLevel;
	  calcPower(); // calculate accordant tx power (in dBm)
  }

  uint8_t CC2420Phy::GetPowerLevel()
  {
	  return m_txPowerLevel;
  }

  double CC2420Phy::GetPowerdBm()
  {
	  return m_txPowerDbm;
  }



  uint8_t CC2420Phy::GetCCAMode()
  {
	 return m_CCA_Mode;
  }

  uint8_t CC2420Phy::GetCCAHyst()
  {
	  return m_CCA_HYST;
  }

  bool CC2420Phy::GetTxTurnaround()
  {
	  return m_txTurnAround;
  }

  bool CC2420Phy::GetAutoCrc()
  {
	  return m_autoCrc;
  }

  uint8_t CC2420Phy::GetPreambleLength()
  {
	  return m_preambleLength;
  }

  uint16_t CC2420Phy::GetSyncWord()
  {
	  return m_syncWord;
  }


  uint16_t CC2420Phy::GetMaxPayloadSize()
  {
  	  return m_MaxPayloadSize;
  }



  void CC2420Phy::SetCCAMode(uint8_t ccaMode)
  {
	  NS_ASSERT_MSG(ccaMode==1 || ccaMode==2 || ccaMode==3,
	  	  			  "CCA mode can only have values 1, 2 or 3!");
	  m_CCA_Mode = ccaMode;
  }

  void CC2420Phy::SetCCAHyst(uint8_t ccaHyst)
  {
	  NS_ASSERT_MSG(ccaHyst>=0 && ccaHyst<=7,
	  			  "CCA hysteresis can only have values between 0 and 7!");
	  m_CCA_HYST = ccaHyst;
  }

  void CC2420Phy::SetTxTurnaround(bool txTurnAround)
  {
	  m_txTurnAround = txTurnAround;
  }

  void CC2420Phy::SetAutoCrc(bool autoCrc)
  {
	  m_autoCrc = autoCrc;
  }

  void CC2420Phy::SetPreambleLength(uint8_t preambleLength)
  {
	  NS_ASSERT_MSG(preambleLength>=0 && preambleLength<=15,
			  "Preamble length can only have values between 0 and 15!");
	  m_preambleLength = preambleLength;
  }

  void CC2420Phy::SetSyncWord(uint16_t syncWord)
  {
	m_syncWord = syncWord;
  }

  void CC2420Phy::SetCSThreshold(int8_t ccaThr)
  {
	  // -45 is the internal offset of the transceiver
	  m_CCA_THR = (int32_t) ccaThr - 45;
  }




  void CC2420Phy::calcPower(){
    if(m_txPowerLevel>30){
        m_txPowerDbm = 0;
    }else if(m_txPowerLevel<4){
        m_txPowerDbm = -25;
    }else{
        //interpolated with eureqa (http://creativemachines.cornell.edu/eureqa)
        m_txPowerDbm = -39.47
            +6.371 * m_txPowerLevel
            -0.6207 * pow((double)m_txPowerLevel,2)
            +0.03925 * pow((double)m_txPowerLevel,3)
            -0.001557 * pow((double)m_txPowerLevel,4)
            +3.521 * pow((double)10,-5) * pow((double)m_txPowerLevel,5)
            -3.391 * pow((double)10,-7) * pow((double)m_txPowerLevel,6);
    }
  }
} /* namespace ns3 */
