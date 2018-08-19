#include "cc2420-channel.h"
#include "cc2420-phy.h"
#include "cripple-tag.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

NS_LOG_COMPONENT_DEFINE ("CC2420Channel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (CC2420Channel);


//max propagation Time 0.333564095 us (100m/c = ~333.5 ns)
Time CC2420Channel::max_propTime = NanoSeconds(334);


TypeId 
CC2420Channel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CC2420Channel")
    .SetParent<Channel> ()
    .AddConstructor<CC2420Channel> ()
  ;
  return tid;
}

CC2420Channel::CC2420Channel ()
{
  //initialize Timers for message on SubChannels
  for (uint8_t i = 0; i <= 15; i++){
      m_subChannelMessageTimer[i].SetFunction(&CC2420Channel::messageExpired,
                                              this
                                             );
      //Set Argument for CC2420Channel::messageExpired(uint8_t channelNo)
      m_subChannelMessageTimer[i].SetArguments(i);
  }
}

CC2420Channel::~CC2420Channel ()
{
  //delete Timers
  for (uint8_t i = 0; i <= 15; i++){
      m_subChannelMessageTimer[i].~Timer();
  }
}

void
CC2420Channel::SetPropagationLossModel (Ptr<PropagationLossModel> loss)
{
  m_loss = loss;
}
void
CC2420Channel::SetPropagationDelayModel (Ptr<PropagationDelayModel> delay)
{
  m_delay = delay;
}

void
CC2420Channel::messageExpired(uint8_t channelNo){
  m_subchannels[channelNo].pop();

  //are there any messages left
  if (m_subchannels[channelNo].size()>0){
      //so set the Timer again
      m_subChannelMessageTimer[channelNo].Schedule(
          calcRemainingTime(m_subchannels[channelNo].top())
          );
  }
}

Time
CC2420Channel::calcRemainingTime(const Message msg) const{
  //calculate remaining Time for shortest/oldest message
  Time remTime = max_propTime // max. Propagation Time
                +msg.transmissionTime //+Transmisson
                -(Simulator::Now()-msg.start);
              //-(  ^Now  - Start Time of message)
  return remTime;
}

void
CC2420Channel::doSend( Ptr<CC2420Phy> sender,
                       Ptr<const Packet> packet,
                       double txPowerDbm,
//                     Ptr<MobilityModel> senderMobility,
                       Ptr<CC2420Phy> receiver,
                       Time elapsed = NanoSeconds(0)) const
{
  // sender mobility can be fetched from sender, so there is no need for a
  // separate parameter
  Ptr<MobilityModel> senderMobility =
		  	  	sender->GetDevice()->GetNode()->GetObject<MobilityModel> ();
  NS_ASSERT (senderMobility != 0);

  Ptr<MobilityModel> receiverMobility =
                receiver->GetDevice()->GetNode()->GetObject<MobilityModel> ();
  NS_ASSERT (receiverMobility != 0);

  Time prop_delay = m_delay->GetDelay (senderMobility, receiverMobility);
  //is some time already elapsed (only neccessary for change of channel number)
  prop_delay = prop_delay - elapsed;

  double rxPowerDbm = m_loss->CalcRxPower (txPowerDbm,
                                           senderMobility,
                                           receiverMobility);
  NS_LOG_DEBUG ("propagation: txPower=" << txPowerDbm << "dBm, "
                "rxPower=" << rxPowerDbm << "dBm, " <<
                "distance=" << senderMobility->GetDistanceFrom(
                                                               receiverMobility
                                                              )<< "m, " <<
                "propagation delay=" << prop_delay);

  Simulator::ScheduleWithContext (receiver->GetDevice()->GetNode ()->GetId (),
      prop_delay,
                                  &CC2420Phy::Receive,
                                  receiver,
                                  packet->Copy (), rxPowerDbm);
}

void
CC2420Channel::Send (Ptr<CC2420Phy> sender,
                     Ptr<const Packet> packet,
                     double txPowerDbm,
                     Time transmission_time)
{
  struct CC2420Channel::Message msg = {sender,
                                       packet,
                                       txPowerDbm,
                                       Simulator::Now(), //Start Time
                                       transmission_time };

  //actual channel number
  uint8_t channelNo = sender->GetChannelNumber();

  //store new message
  m_subchannels[channelNo].push(msg);

  Time remTime = calcRemainingTime(m_subchannels[channelNo].top());

  //set Timer
  if (!m_subChannelMessageTimer[channelNo].IsRunning()){
      //is not running
      //->set for shortest packet
      m_subChannelMessageTimer[channelNo].Schedule(remTime);
  }else{
      //Timer is already running

      if (m_subChannelMessageTimer[channelNo].GetDelayLeft()>remTime){
          //there is a shorter/older message
          //so reset the Timer
          m_subChannelMessageTimer[channelNo].Cancel();
          m_subChannelMessageTimer[channelNo].Schedule(remTime);
      }
  }

  //Ptr<MobilityModel> senderMobility =
  //                sender->GetDevice()->GetNode()->GetObject<MobilityModel> ();
  //NS_ASSERT (senderMobility != 0);

  for (std::vector<Ptr<CC2420Phy> >::const_iterator i = m_phyList.begin ();
      i != m_phyList.end ();
      ++i)
    {
      Ptr<CC2420Phy> receiver = *i;
      if (sender != receiver &&
          sender->GetChannelNumber() == receiver->GetChannelNumber())
        {
          doSend(sender, packet, txPowerDbm, receiver, Seconds(0));
        }
    }
}

uint32_t
CC2420Channel::GetNDevices (void) const
{
  return m_phyList.size ();
}
Ptr<NetDevice>
CC2420Channel::GetDevice (uint32_t i) const
{
  return m_phyList[i]->GetDevice ()->GetObject<NetDevice> ();
}

void
CC2420Channel::Add (Ptr<CC2420Phy> phy)
{
  m_phyList.push_back (phy);
  m_size = GetNDevices();
}

void
CC2420Channel::NotifyChannelNumberChange (Ptr<CC2420Phy> phy,
                                          uint8_t newChannelNumber)
{
  vector<Message> messages(m_subchannels[newChannelNumber].size());

  //copy messages to vector
  std::copy( &(m_subchannels[newChannelNumber].top()),
      &(m_subchannels[newChannelNumber].top())
        +m_subchannels[newChannelNumber].size(),
      &messages[0]
                );

  for (vector<Message>::iterator msg=messages.begin();
       msg < messages.end();
       msg++)
  {
      //Get Position of Sender
      Ptr<MobilityModel> senderMobility =
          msg->sender->GetDevice()->GetNode()->GetObject<MobilityModel> ();
      NS_ASSERT (senderMobility != 0);

      //Get Position of Receiver
      Ptr<MobilityModel> receiverMobility =
                  phy->GetDevice()->GetNode()->GetObject<MobilityModel> ();
      NS_ASSERT (receiverMobility != 0);

      //calculate Propagation Time
      Time prop_delay = m_delay->GetDelay (senderMobility, receiverMobility);

      //is there any time alread elapsed since start of the packet
      Time elapsed;

      //is the message still in transit (Propagation-Time)?
      if (Simulator::Now() <= (msg->start+prop_delay)){
          //yes, so the message could be fully received
          elapsed = Simulator::Now() - msg->start;
      }else{
          //no, the message has already reached the channel-changing PHY
          //the Propagation Time is completely elapsed

          //cripple the packet
          Ptr<Packet> pkt = msg->pkt->Copy();
          CrippleTag cripple;
          pkt->AddPacketTag(cripple);
          //remove unheard Bytes at the start of the packet
          //#Bytes = TransmissionRate * elapsed Time
          //elapsed Time = Now - StartOfPacket - PropagationTime
          uint32_t removeBytes =
              msg->sender->GetTransmissionRateBytesPerSecond()
              * (Simulator::Now()
                 -msg->start
                 -prop_delay
                ).GetSeconds();
          pkt->RemoveAtStart(removeBytes);

          //start receiving immediately
          //there for: set elapsed Time to Propagation Time
          elapsed = prop_delay;
      }
      //send the packet
      doSend(msg->sender,
             msg->pkt,
             msg->txPowerDbm,
             //senderMobility,
             phy,
             elapsed);
  }
}

} // namespace ns3
