#ifndef CC2420_CHANNEL_H
#define CC2420_CHANNEL_H

#include <vector>
#include <queue>

#include "ns3/channel.h"
#include "ns3/mac48-address.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ns3/timer.h"
#include "ns3/mobility-model.h"

namespace ns3 {

class CC2420Phy;
class NetDevice;
class Packet;
class PropagationLossModel;
class PropagationDelayModel;

// needed for use of vector
using namespace std;

/**
 * \ingroup channel
 * \brief A cc2420 channel, for simple things and testing
 */
class CC2420Channel : public Channel
{
private:
  //max propagation Time 0.333564095 us (100m/c = ~333.5 ns)
  static Time max_propTime;


  struct Message {
    Ptr<CC2420Phy> sender;
    Ptr<const Packet> pkt;
    double txPowerDbm;
    Time start; //now
    Time transmissionTime; // pkt.size()/Uebertragungsrate (Uebertragungsrate = 250kbps = 31250 Bytes/s)

    //max TransmissionTime = 4096 us = (128Bytes)/(31250Bytes/s) = 256 Symbol-Zeiten
    //max propagation Time 0.333564095 us (100m/c = ~333.5 ns)
    //max verbleibendend Zeit = max_Trans+max_prop
  };

  class CompareMessage {
  public:
      bool operator()(const Message& m1, const Message& m2) const
      {
         return (m1.start+m1.transmissionTime < m2.start+m2.transmissionTime);
      }
  };

  Time calcRemainingTime(const Message msg) const;

  mutable std::priority_queue<Message, std::vector<Message>, CompareMessage> m_subchannels[16];
  Timer m_subChannelMessageTimer[16];

  void messageExpired(uint8_t channelNo);

  void doSend(Ptr<CC2420Phy> sender,
              Ptr<const Packet> packet,
              double txPowerDbm,
//            Ptr<MobilityModel> senderMobility,
              Ptr<CC2420Phy> receiver,
              Time elapsed) const;


  std::vector<Ptr<CC2420Phy> > m_phyList;

  Ptr<PropagationLossModel> m_loss;
  Ptr<PropagationDelayModel> m_delay;
  uint32_t m_size;


public:
  static TypeId GetTypeId (void);
  CC2420Channel ();
  ~CC2420Channel ();

  /**
   * \param loss the new propagation loss model.
   */
  void SetPropagationLossModel (Ptr<PropagationLossModel> loss);
  /**
   * \param delay the new propagation delay model.
   */
  void SetPropagationDelayModel (Ptr<PropagationDelayModel> delay);

  /**
   * \param sender the device from which the packet is originating.
   * \param packet the packet to send
   * \param txPowerDbm the tx power associated to the packet
   *
   * This method should not be invoked by normal users. It is
   * currently invoked only from WifiPhy::Send. YansWifiChannel
   * delivers packets only between PHYs with the same m_channelNumber,
   * e.g. PHYs that are operating on the same channel.
   */
  void Send (Ptr<CC2420Phy> sender, Ptr<const Packet> packet, double txPowerDbm, Time transmission_time);

  void Add (Ptr<CC2420Phy> phy);

  /**
   * \param phy the device which is changing the channel.
   * \param newChannelNumber the new channel number
   *
   * This method should not be invoked by normal users. It is
   * currently invoked only from CC2420Phy::ChangeChannelNumber.
   * CC2420Channel sends any message on air to the new member (phy) of
   * the subchannel (newChannelNumber),  so PHY could correctly determine the
   * CCA-Status.
   * CC2420Channel will deliver packets only between PHYs with the same
   * m_channelNumber.
   */
  void NotifyChannelNumberChange (Ptr<CC2420Phy> phy, uint8_t newChannelNumber);

  // inherited from ns3::Channel
  virtual uint32_t GetNDevices (void) const;
  virtual Ptr<NetDevice> GetDevice (uint32_t i) const;

};

} // namespace ns3

#endif /* CC2420_CHANNEL_H */
