#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "packet-sink-cc2420.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PacketSinkCC2420");
NS_OBJECT_ENSURE_REGISTERED (PacketSinkCC2420);

TypeId 
PacketSinkCC2420::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PacketSinkCC2420")
    .SetParent<Application> ()
    .AddConstructor<PacketSinkCC2420> ()
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&PacketSinkCC2420::m_rxTrace))
  ;
  return tid;
}

PacketSinkCC2420::PacketSinkCC2420 ()
{
  NS_LOG_FUNCTION (this);
  m_totalRx = 0;
}

PacketSinkCC2420::~PacketSinkCC2420()
{
  NS_LOG_FUNCTION (this);
}

uint32_t PacketSinkCC2420::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

void PacketSinkCC2420::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  // chain up
  Application::DoDispose ();
}


// Application Methods
void PacketSinkCC2420::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // get accordant node of this application and get first NetDevice of this
  // node (only the use of one NetDevice is supported at the moment)
  // do this in StartApplication, because node is not yet assigned when
  // application is constructed
  netDevice = GetNode()->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
  netDevice->SetMessageCallback(MakeCallback(&PacketSinkCC2420::HandleRead, this));

  uint8_t channelNo = 12; // channel number is changed from default value 11 to 12
  uint8_t power = 31; // power remains on default value of 31
  Ptr<CC2420Message> msg = CreateObject<CC2420Setup>(channelNo, power);
  //construct Config message (default power, changed channel)
  netDevice->descendingSignal(msg);

  // request current status
  netDevice->descendingSignal(CreateObject<CC2420StatusReq>());
}


void PacketSinkCC2420::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
}


bool PacketSinkCC2420::HandleRead (Ptr<CC2420Message> msg)
{
  NS_LOG_FUNCTION (this << msg);
  //NS_LOG_INFO ("Received message from CC2420InterfaceNetDevice");

  if(msg==NULL){
  	NS_LOG_ERROR("Message not correctly received!");
  	return false;
  }


  Ptr<CC2420Recv> recvMsg = DynamicCast<CC2420Recv>(msg);
  if(recvMsg){
	  m_totalRx += recvMsg->getSize();
      std::cout << "At time " << Simulator::Now ().GetSeconds ()
              << "s packet sink cc2420 received " << recvMsg->getSize() << " bytes"
              //<< " bytes with CRC=" << (recvMsg->getCRC()?"true":"false")
              //<< " and RSSI=" << recvMsg->getRSSI()
              << "; total Rx " << m_totalRx << " bytes" << std::endl;
	  return true;

  } else {
	  Ptr<CC2420Cca> ccaMsg = DynamicCast<CC2420Cca>(msg);
      if(ccaMsg){
    	  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
    			  << "s packet sink cc2420 received CC2420Cca message with value "
    			  << (ccaMsg->getCcaValue()?"true":"false"));
    	  return true;

      } else {
    	  Ptr<CC2420Sending> sendingMsg = DynamicCast<CC2420Sending>(msg);
          if(sendingMsg){
        	  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
        			  << "s packet sink cc2420 received CC2420Sending message with value "
        	      	  << (sendingMsg->getSending()?"true":"false"));
        	  return true;

          } else {
        	  Ptr<CC2420SendFinished> sfMsg = DynamicCast<CC2420SendFinished>(msg);
              if(sfMsg){
            	  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
            			  << "s packet sink cc2420 received CC2420SendFinished message");
            	  return true;

              } else {
            	  Ptr<CC2420StatusResp> respMsg = DynamicCast<CC2420StatusResp>(msg);
            	  if(respMsg){
            		  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
            				  << "s packet sink cc2420 received CC2420StatusResp message with values"
            		          << " CCA mode=" << (int) respMsg->getCcaMode()
            		          << ", CCA hysteresis=" << (int) respMsg->getCcaHysteresis()
            		          << ", CCA threshold=" << (int) respMsg->getCcaThreshold()
            		          << ", long TX turnaround=" << (respMsg->getTxTurnaround()?"true":"false")
            		          << ", automatic CRC=" << (respMsg->getAutoCrc()?"true":"false")
            		          << ", preamble length=" << (int) respMsg->getPreambleLength()
            		          << ", sync word=0x" << std::hex << (int) respMsg->getSyncWord() << std::dec
            		          << ", channel=" << (int) respMsg->getChannel()
            		          << ", power=" << (int) respMsg->getPower());
            		  return true;
            	  } else {
            		  //unknown message or NULL-Pointer
            		  NS_LOG_ERROR("CC2420Message is of an unknown type!");
            		  return false;
            	  } //unknown
              } //status response
          } // send finished
      } // sending
  } // receive

  return false; // something went wrong
}

} // Namespace ns3
