#include "cc2420-interface-net-device.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/packet.h"
#include <typeinfo> // For std::bad_cast
#include <iostream> // For std::cerr, etc.

#include "cc2420-channel.h"
#include "cc2420-phy.h"

NS_LOG_COMPONENT_DEFINE ("CC2420InterfaceNetDevice");

namespace ns3
{
  NS_OBJECT_ENSURE_REGISTERED (CC2420InterfaceNetDevice);

  TypeId CC2420InterfaceNetDevice::GetTypeId(void)
  {
      static TypeId tid = TypeId("ns3::CC2420InterfaceNetDevice")
          .SetParent<CC2420NetDevice>()
          .AddConstructor<CC2420InterfaceNetDevice>();
//          .AddAttribute("ReceiveErrorModel",
//                        "The receiver error model used to simulate packet loss",
//                        PointerValue(),
//                        MakePointerAccessor(&CC2420InterfaceNetDevice::m_receiveErrorModel),
//                        MakePointerChecker<ErrorModel>())
//          .AddTraceSource("PhyRxDrop",
//                        "Trace source indicating a packet has been dropped by the device during reception",
//                        MakeTraceSourceAccessor(&CC2420InterfaceNetDevice::m_phyRxDropTrace));
      return tid;
  }


  CC2420InterfaceNetDevice::CC2420InterfaceNetDevice()
  {
  }


  void CC2420InterfaceNetDevice::descendingSignal(Ptr<CC2420Message> msg)
  {
    try{
      Ptr<CC2420Send> send = DynamicCast<CC2420Send>(msg);
      if(send){
    	  // construct ns3 packet
    	  Ptr<Packet> p = Create<Packet> (send->getMessageData(), send->getSize());
    	  // the protocol number is not important for us, so it can be set to 0
    	  Send(p, send->getSendWithCCA(), GetBroadcast(), 0);

      } else {
        Ptr<CC2420StatusReq> query = DynamicCast<CC2420StatusReq>(msg);
        if(query){
        	// construct status message with the following information:
        	// ccaMode, ccaHyst, ccaThr, channelNr, sendPower, txTurnaround, autoCrc, preambleLen, syncWord
        	Ptr<CC2420StatusResp> status = CreateObject<CC2420StatusResp>(
        			m_phy->GetCCAMode(), m_phy->GetCCAHyst(), m_phy->GetCSThreshold(),
        			m_phy->GetChannelNumber(), m_phy->GetPowerLevel(),
        			m_phy->GetTxTurnaround(), m_phy->GetAutoCrc(),
        			m_phy->GetPreambleLength(), m_phy->GetSyncWord());

        	messageCb(status); // return status via callback to framework

        } else {
          Ptr<CC2420Config> config = DynamicCast<CC2420Config>(msg);
          if(config){
        	  // change ccaMode, ccaHyst, ccaThr, txTurnaround, autoCrc, preambleLen, syncWord
        	  // in Phy layer
        	  m_phy->SetCCAMode(config->getCcaMode());
        	  m_phy->SetCCAHyst(config->getCcaHysteresis());
        	  m_phy->SetCSThreshold(config->getCcaThreshold());
        	  m_phy->SetTxTurnaround(config->getTxTurnaround());
        	  m_phy->SetAutoCrc(config->getAutoCrc());
        	  m_phy->SetPreambleLength(config->getPreambleLength());
        	  m_phy->SetSyncWord(config->getSyncWord());

          } else {
        	Ptr<CC2420Setup> setup = DynamicCast<CC2420Setup>(msg);
            if(setup){
            	// change channel number and sending power in Phy layer
            	m_phy->ChangeChannelNumber(setup->getChannel());
            	m_phy->SetPowerLevel(setup->getPower());

            } else {
                //unknown message or NULL-Pointer
            	NS_LOG_ERROR("Message is of an unknown type!");
            }//unknown
          }//setup
        }//config
      }//query

    } catch (const std::bad_cast& e){
      /* bad_cast: If dynamic_cast is used to convert to a reference type
      * and the conversion is not possible, an exception of type bad_cast
      * is thrown instead.
      */
      std::cerr << e.what() << std::endl;
      std::cerr << "This object is not of type CC2420Send, CC2420StatusReq,"
                <<"CC2420Config or CC2420Setup" << std::endl;
    }
  }


  void CC2420InterfaceNetDevice::SetMessageCallback (MessageCallback cb)
  {
	  messageCb = cb;
  }


  bool CC2420InterfaceNetDevice::Receive (Ptr<Packet> packet, bool crc, double rxPowerDbm)
  {
	  // preserve original behavior; only forward packet if it is received by
	  // original Receive method
	  if(CC2420NetDevice::Receive(packet,crc,rxPowerDbm)){
		  // copy data of packet
		  uint32_t size = packet->GetSize();
		  uint8_t data[size];
		  packet->CopyData(data,size);

		  Ptr<CC2420Recv> recvMsg = CreateObject<CC2420Recv>(data, size, crc, rxPowerDbm);
		  if(!messageCb.IsNull()){
			  messageCb(recvMsg); // forward packet via callback to application
		  }
		  return true;
	  }
	  return false;
  }

  void CC2420InterfaceNetDevice::NotifyTxStart(void)
  {
	  CC2420NetDevice::NotifyTxStart(); // preserve original behavior

	  Ptr<CC2420Sending> sendingMsg = CreateObject<CC2420Sending>(true);
	  if(!messageCb.IsNull()){
		  messageCb(sendingMsg); // forward sending value via callback to application
	  }
  }

  void CC2420InterfaceNetDevice::NotifyTxEnd(void)
  {
	  CC2420NetDevice::NotifyTxEnd(); // preserve original behavior

	  Ptr<CC2420SendFinished> sendFinishMsg = CreateObject<CC2420SendFinished>();
	  if(!messageCb.IsNull()){
		  messageCb(sendFinishMsg); // indicate end of transmission via callback to application
	  }
  }

  void CC2420InterfaceNetDevice::NotifyTxFailure(const char *msg)
  {
	  CC2420NetDevice::NotifyTxFailure(msg); // preserve original behavior

	  Ptr<CC2420Sending> sendingMsg = CreateObject<CC2420Sending>(false);
	  if(!messageCb.IsNull()){
		  messageCb(sendingMsg); // forward sending value via callback to application
	  }
  }

  void CC2420InterfaceNetDevice::NotifyCcaChange(bool value)
  {
 	  CC2420NetDevice::NotifyCcaChange(value); // preserve original behavior

 	 Ptr<CC2420Cca> ccaMsg = CreateObject<CC2420Cca>(value);
 	 if(!messageCb.IsNull()){
 		 messageCb(ccaMsg); // forward CCA value via callback to application
 	 }
  }


} /* namespace ns3 */
