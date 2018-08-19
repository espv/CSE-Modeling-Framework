#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "onoff-cc2420-application.h"
#include "ns3/string.h"
#include "ns3/pointer.h"

NS_LOG_COMPONENT_DEFINE ("OnOffCC2420Application");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (OnOffCC2420Application);

TypeId
OnOffCC2420Application::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OnOffCC2420Application")
    .SetParent<Application> ()
    .AddConstructor<OnOffCC2420Application> ()
    .AddAttribute ("DataRate", "The data rate in on state.",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&OnOffCC2420Application::m_cbrRate),
                   MakeDataRateChecker ())
    .AddAttribute ("PacketSize", "The size of packets sent in on state",
                   UintegerValue (512),
                   MakeUintegerAccessor (&OnOffCC2420Application::m_pktSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("OnTime", "A RandomVariableStream used to pick the duration of the 'On' state.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&OnOffCC2420Application::m_onTime),
                   MakePointerChecker <RandomVariableStream>())
    .AddAttribute ("OffTime", "A RandomVariableStream used to pick the duration of the 'Off' state.",
                   StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor (&OnOffCC2420Application::m_offTime),
                   MakePointerChecker <RandomVariableStream>())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. Once these bytes are sent, "
                   "no packet is sent again, even in on state. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&OnOffCC2420Application::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&OnOffCC2420Application::m_txTrace))
  ;
  return tid;
}


OnOffCC2420Application::OnOffCC2420Application ()
  : m_residualBits (0),
    m_lastStartTime (Seconds (0)),
    m_totBytes (0)
{
  NS_LOG_FUNCTION (this);
}

OnOffCC2420Application::~OnOffCC2420Application()
{
  NS_LOG_FUNCTION (this);
}

void
OnOffCC2420Application::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

int64_t
OnOffCC2420Application::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_onTime->SetStream (stream);
  m_offTime->SetStream (stream + 1);
  return 2;
}

void
OnOffCC2420Application::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  // chain up
  Application::DoDispose ();
}

// Application Methods
void OnOffCC2420Application::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // get accordant node of this application and get first NetDevice of this
  // node (only the use of one NetDevice is supported at the moment)
  // do this in StartApplication, because node is not yet assigned when
  // application is constructed
  netDevice = GetNode()->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
  netDevice->SetMessageCallback(MakeCallback(&OnOffCC2420Application::HandleRead, this));

  uint8_t channelNo = 12; // channel number is changed from default value 11 to 12
  uint8_t power = 31; // power remains on default value of 31
  Ptr<CC2420Message> msg = CreateObject<CC2420Setup>(channelNo, power);
  //construct Config message (default power, changed channel)
  netDevice->descendingSignal(msg);

  // request current status
  netDevice->descendingSignal(CreateObject<CC2420StatusReq>());

  // Insure no pending event
  CancelEvents ();
  ScheduleStartEvent ();
}

void OnOffCC2420Application::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  CancelEvents ();
}

void OnOffCC2420Application::CancelEvents ()
{
  NS_LOG_FUNCTION (this);

  if (m_sendEvent.IsRunning ())
    { // Cancel the pending send packet event
      // Calculate residual bits since last packet sent
      Time delta (Simulator::Now () - m_lastStartTime);
      int64x64_t bits = delta.To (Time::S) * m_cbrRate.GetBitRate ();
      m_residualBits += bits.GetHigh ();
    }
  Simulator::Cancel (m_sendEvent);
  Simulator::Cancel (m_startStopEvent);
}

// Event handlers
void OnOffCC2420Application::StartSending ()
{
  NS_LOG_FUNCTION (this);
  m_lastStartTime = Simulator::Now ();
  ScheduleNextTx ();  // Schedule the send packet event
  ScheduleStopEvent ();
}

void OnOffCC2420Application::StopSending ()
{
  NS_LOG_FUNCTION (this);
  CancelEvents ();

  ScheduleStartEvent ();
}

// Private helpers
void OnOffCC2420Application::ScheduleNextTx ()
{
  NS_LOG_FUNCTION (this);

  if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      uint32_t bits = m_pktSize * 8 - m_residualBits;
      NS_LOG_LOGIC ("bits = " << bits);
      Time nextTime (Seconds (bits /
                              static_cast<double>(m_cbrRate.GetBitRate ()))); // Time till next packet
      NS_LOG_LOGIC ("nextTime = " << nextTime);
      m_sendEvent = Simulator::Schedule (nextTime,
              &OnOffCC2420Application::SendPacket, this);
    }
  else
    { // All done, cancel any pending events
      StopApplication ();
    }
}

void OnOffCC2420Application::ScheduleStartEvent ()
{  // Schedules the event to start sending data (switch to the "On" state)
  NS_LOG_FUNCTION (this);

  Time offInterval = Seconds (m_offTime->GetValue ());
  NS_LOG_LOGIC ("start at " << offInterval);
  m_startStopEvent = Simulator::Schedule (offInterval, &OnOffCC2420Application::StartSending, this);
}

void OnOffCC2420Application::ScheduleStopEvent ()
{  // Schedules the event to stop sending data (switch to "Off" state)
  NS_LOG_FUNCTION (this);

  Time onInterval = Seconds (m_onTime->GetValue ());
  NS_LOG_LOGIC ("stop at " << onInterval);
  m_startStopEvent = Simulator::Schedule (onInterval, &OnOffCC2420Application::StopSending, this);
}

void OnOffCC2420Application::SendPacket ()
{
  NS_LOG_FUNCTION (this);

  //NS_ASSERT (m_sendEvent.IsExpired ());  // I don't think we care about that by Espen
  //Ptr<Packet> packet = Create<Packet> (m_pktSize);
  //m_txTrace (packet);

  // payload is filled with 0 values (the direct use of a packet with zero-filled
  // payload is not possible here, since the packet is constructed by the
  // CC2420InterfaceNetDevice)
  uint8_t nullBuffer[m_pktSize];
  for(uint32_t i=0; i<m_pktSize; i++) nullBuffer[i] = 0;

  // send with CCA
  Ptr<CC2420Send> msg = CreateObject<CC2420Send>(nullBuffer, m_pktSize, true);

  m_totBytes += m_pktSize;

  NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
          << "s on-off-cc2420 application sent " <<  m_pktSize << " bytes"
          << " total Tx " << m_totBytes << " bytes");

  netDevice->descendingSignal(msg);

  m_lastStartTime = Simulator::Now ();
  m_residualBits = 0;
  ScheduleNextTx ();
}

void OnOffCC2420Application::ReSend() {
    uint8_t nullBuffer[m_pktSize];
    for(uint32_t i=0; i<m_pktSize; i++) nullBuffer[i] = 0;

    // send with CCA
    Ptr<CC2420Send> msg = CreateObject<CC2420Send>(nullBuffer, m_pktSize, true);

    NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
            << "s on-off-cc2420 application re-sent " <<  m_pktSize << " bytes"
            << " total Tx " << m_totBytes << " bytes");
    netDevice->descendingSignal(msg);

    m_lastStartTime = Simulator::Now ();
    m_residualBits = 0;
}


bool OnOffCC2420Application::HandleRead (Ptr<CC2420Message> msg)
{
  NS_LOG_FUNCTION (this << msg);
  //NS_LOG_INFO ("Received message from CC2420InterfaceNetDevice");

  // What does it mean that it has not been received correctly? Bad CRC?
  if(msg==NULL){
    NS_LOG_ERROR("Message not correctly received!");
    return false;
  }


  Ptr<CC2420Recv> recvMsg = DynamicCast<CC2420Recv>(msg);
  if(recvMsg){
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
              << "s on-off-cc2420 application received " << recvMsg->getSize()
              << " bytes with CRC=" << (recvMsg->getCRC()?"true":"false")
              << " and RSSI=" << recvMsg->getRSSI() << " bytes");
      return true;

  } else {
      Ptr<CC2420Cca> ccaMsg = DynamicCast<CC2420Cca>(msg);
      if(ccaMsg){
          NS_LOG_INFO ("At time " << Simulator::Now ()
                  << "s on-off-cc2420 application received CC2420Cca message with value "
                  << (ccaMsg->getCcaValue()?"true":"false"));
          return true;

      } else {
          Ptr<CC2420Sending> sendingMsg = DynamicCast<CC2420Sending>(msg);
          if(sendingMsg){
              if (!sendingMsg->getSending ()) {
                  Simulator::Schedule (Seconds(0.0025),  // Espen added the +Seconds(1)
                                       &OnOffCC2420Application::ReSend, this);
              }
              NS_LOG_INFO ("At time " << Simulator::Now ()
                      << "s on-off-cc2420 application received CC2420Sending message with value "
                      << (sendingMsg->getSending()?"true":"false"));
              return true;

          } else {
              Ptr<CC2420SendFinished> sfMsg = DynamicCast<CC2420SendFinished>(msg);
              if(sfMsg){
                  NS_LOG_INFO ("At time " << Simulator::Now ()
                          << "s on-off-cc2420 application received CC2420SendFinished message");
                  return true;

              } else {
                  Ptr<CC2420StatusResp> respMsg = DynamicCast<CC2420StatusResp>(msg);
                  if(respMsg){
                      NS_LOG_INFO ("At time " << Simulator::Now ()
                              << "s on-off-cc2420 application received CC2420StatusResp message with values"
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
                      NS_LOG_INFO ("CC2420Message is of an unknown type!");
                      return false;
                  } //unknown
              } //status response
          } // send finished
      } // sending
  } // receive

  return false; // something went wrong
}

} // Namespace ns3
