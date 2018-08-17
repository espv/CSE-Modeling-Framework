#include "cc2420-net-device.h"
#include "cc2420-channel.h"
#include "cc2420-phy.h"
#include "address-tag.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/trace-source-accessor.h"


NS_LOG_COMPONENT_DEFINE ("CC2420NetDevice");

namespace ns3 {
    NS_OBJECT_ENSURE_REGISTERED (CC2420NetDevice);

    TypeId CC2420NetDevice::GetTypeId(void)
    {
      static TypeId tid = TypeId("ns3::CC2420NetDevice")
        .SetParent<NetDevice>()
        .AddConstructor<CC2420NetDevice>()
        .AddAttribute("ReceiveErrorModel",
          "The receiver error model used to simulate packet loss",
          PointerValue(),
          MakePointerAccessor(&CC2420NetDevice::m_receiveErrorModel),
          MakePointerChecker<ErrorModel>()
        ).AddTraceSource("PhyRxDrop",
            "Trace source indicating a packet has been dropped by the device during reception",
            MakeTraceSourceAccessor(&CC2420NetDevice::m_phyRxDropTrace));
      return tid;
    }

    CC2420NetDevice::CC2420NetDevice()
    :m_node(0), m_ifIndex(0)
    {
        m_listErrorModel = CreateObject<ListErrorModel>();
        m_phy = CreateObject<CC2420Phy>();
        m_phy->SetDevice(this);

        m_mtu = m_phy->GetMaxPayloadSize();
    }


    bool CC2420NetDevice::Receive(Ptr<Packet> packet, bool crc, double rxPowerDbm)
    {
        AddressTag adrTag;
        packet->RemovePacketTag(adrTag);
        Mac48Address to = Mac48Address::ConvertFrom(adrTag.GetDestination());

        NetDevice::PacketType packetType;
        if( m_listErrorModel->IsCorrupt(packet) ||
           (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
          )
        {
            m_phyRxDropTrace(packet);
            return false;
        }
        if(to == m_address){
            packetType = NetDevice::PACKET_HOST;
        }else
            if(to.IsBroadcast()){
                packetType = NetDevice::PACKET_HOST;
            }else
                if(to.IsGroup()){
                    packetType = NetDevice::PACKET_MULTICAST;
                }else{
                    packetType = NetDevice::PACKET_OTHERHOST;
                }

        if(!m_rxCallback.IsNull()){
            m_rxCallback(this, packet, adrTag.GetProtocolNumber(), adrTag.GetDestination());
        }

        if(!m_promiscCallback.IsNull()){
            m_promiscCallback(this, packet, adrTag.GetProtocolNumber(), adrTag.GetSource(), adrTag.GetDestination(), packetType);
        }
        return true;
    }


    bool CC2420NetDevice::Send (Ptr<Packet> packet, bool checkCCA, const Address& dest, uint16_t protocolNumber)
    {
    	return SendFrom(packet, checkCCA, m_address, dest, protocolNumber);
    }

    bool CC2420NetDevice::SendFrom (Ptr<Packet> packet, bool checkCCA, const Address& source, const Address& dest, uint16_t protocolNumber)
    {
        AddressTag adrTag;
    	adrTag.SetData(source, dest, protocolNumber);
    	packet->AddPacketTag(adrTag);
    	return m_phy->Send(packet, checkCCA);
    }



    bool CC2420NetDevice::Send(Ptr<Packet> packet, const Address & dest, uint16_t protocolNumber)
    {
        return SendFrom(packet, m_address, dest, protocolNumber);
    }

    bool CC2420NetDevice::SendFrom(Ptr<Packet> packet, const Address & source, const Address & dest, uint16_t protocolNumber)
    {
    	// send with CCA
        return SendFrom(packet, true, source, dest, protocolNumber);
    }

    void CC2420NetDevice::SetChannel(Ptr<CC2420Channel> channel)
    {
        return m_phy->SetChannel(channel);
    }

    void CC2420NetDevice::SetReceiveErrorModel(Ptr<ErrorModel> em)
    {
        m_receiveErrorModel = em;
    }


    void CC2420NetDevice::NotifyCcaChange(bool value)
    {
        //NS_LOG_UNCOND("Medium is " << (value?"free.":"busy.") << " Device: "<< this->GetAddress());
    	NS_LOG_INFO("Medium is " << (value?"free.":"busy.") << " Device: "<< this->GetAddress());
    }
    std::list<uint32_t> CC2420NetDevice::GetPacketErrorList(void) const
    {
        return m_listErrorModel->GetList();
    }

    void CC2420NetDevice::SetPacketErrorList(const std::list<uint32_t> & packetlist)
    {
        m_listErrorModel->SetList(packetlist);
    }

    void CC2420NetDevice::NotifyTxStart(void)
    {
        //NS_LOG_UNCOND("TXStart " << " Sender: "<< this->GetAddress());
    	NS_LOG_INFO("TXStart " << " Sender: "<< this->GetAddress());
    }

    void CC2420NetDevice::NotifyTxEnd(void)
    {
        //NS_LOG_UNCOND("TXEnd " << " Sender: "<< this->GetAddress());
    	NS_LOG_INFO("TXEnd " << " Sender: "<< this->GetAddress());
    }

    void CC2420NetDevice::NotifyTxFailure(const char *msg)
    {
      //NS_LOG_UNCOND("TXFailure: " << msg << " Sender: "<< this->GetAddress());
      NS_LOG_INFO("TXFailure: " << msg << " Sender: "<< this->GetAddress());
    }

/*-----------------------------------------------------------------------------
 *
 * inherited from NetDevice base class.
 *
 *------------------------------------------------------------------------------
 */
    void CC2420NetDevice::SetIfIndex(const uint32_t index)
    {
        m_ifIndex = index;
    }

    uint32_t CC2420NetDevice::GetIfIndex(void) const
    {
        return m_ifIndex;
    }

    Ptr<Channel> CC2420NetDevice::GetChannel(void) const
    {
        return m_phy->GetChannel();
    }

    void CC2420NetDevice::SetAddress(Address address)
    {
        m_address = Mac48Address::ConvertFrom(address);
    }

    Address CC2420NetDevice::GetAddress(void) const
    {
        //
        // Implicit conversion from Mac48Address to Address
        //
        return m_address;
    }

    bool CC2420NetDevice::SetMtu(const uint16_t mtu)
    {
        m_mtu = mtu;
        return true;
    }

    uint16_t CC2420NetDevice::GetMtu(void) const
    {
        return m_mtu;
    }

    bool CC2420NetDevice::IsLinkUp(void) const
    {
        return true;
    }

    void CC2420NetDevice::AddLinkChangeCallback(Callback<void> callback)
    {
    }

    bool CC2420NetDevice::IsBroadcast(void) const
    {
        return true;
    }

    Address CC2420NetDevice::GetBroadcast(void) const
    {
        return Mac48Address("ff:ff:ff:ff:ff:ff");
    }

    bool CC2420NetDevice::IsMulticast(void) const
    {
        return false;
    }

    Address CC2420NetDevice::GetMulticast(Ipv4Address multicastGroup) const
    {
        return Mac48Address::GetMulticast(multicastGroup);
    }

    Address CC2420NetDevice::GetMulticast(Ipv6Address addr) const
    {
        return Mac48Address::GetMulticast(addr);
    }

    bool CC2420NetDevice::IsPointToPoint(void) const
    {
        return false;
    }

    bool CC2420NetDevice::IsBridge(void) const
    {
        return false;
    }

    Ptr<Node> CC2420NetDevice::GetNode(void) const
    {
        return m_node;
    }

    void CC2420NetDevice::SetNode(Ptr<Node> node)
    {
        m_node = node;
    }

    bool CC2420NetDevice::NeedsArp(void) const
    {
        return false;
    }

    void CC2420NetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
    {
        m_rxCallback = cb;
    }


void
CC2420NetDevice::DoDispose (void)
{
  m_node = 0;
  m_receiveErrorModel = 0;
  NetDevice::DoDispose ();
}


void
CC2420NetDevice::SetPromiscReceiveCallback (PromiscReceiveCallback cb)
{
  m_promiscCallback = cb;
}

bool
CC2420NetDevice::SupportsSendFrom (void) const
{
  return true;
}

} // namespace ns3
