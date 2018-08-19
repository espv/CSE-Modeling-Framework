#ifndef CC2420_NET_DEVICE_H
#define CC2420_NET_DEVICE_H

#include "ns3/net-device.h"
#include "ns3/mac48-address.h"
#include <stdint.h>
#include <string>
#include "ns3/traced-callback.h"
#include "ns3/error-model.h"

#include "ns3/node.h"

namespace ns3 {

class CC2420Channel;
class CC2420Phy;


/**
 * \ingroup netdevice
 *
 * This device does not have a helper and assumes 48-bit mac addressing;
 * the default address assigned to each device is zero, so you must 
 * assign a real address to use it.  There is also the possibility to
 * add an ErrorModel if you want to force losses on the device.
 * 
 * \brief cc2420 net device for simple things and testing
 */
class CC2420NetDevice : public NetDevice
{
public:
  static TypeId GetTypeId (void);
  CC2420NetDevice ();

  /**
   * This method is called by the phy layer when a packet is received from
   * the channel. It returns true, if the packet was successfully received,
   * and false, if the packet was dropped (due to the error model).
   */
  virtual bool Receive (Ptr<Packet> packet, bool crc, double rxPowerDbm);


  void SetChannel (Ptr<CC2420Channel> channel);

  /**
   * Attach a receive ErrorModel to the CC2420NetDevice.
   *
   * The CC2420NetDevice may optionally include an ErrorModel in
   * the packet receive chain.
   *
   * \see ErrorModel
   * \param em Ptr to the ErrorModel.
   */
  void SetReceiveErrorModel (Ptr<ErrorModel> em);

  /**
   * \return a copy of the underlying list of m_listErrorModel
   */
  std::list<uint32_t> GetPacketErrorList (void) const;
  /**
   * \param packetlist The list of packet uids to error.
   *
   * This method overwrites any previously provided list of of m_listErrorModel.
   */
  void SetPacketErrorList (const std::list<uint32_t> &packetlist);


  /**
   * Indicates the beginning of a transmission.
   */
  virtual void NotifyTxStart(void);

  /**
   * Indicates the end of a transmission.
   */
  virtual void NotifyTxEnd(void);

  /**
   * Indicates that a transmission could not start, because the transmitter
   * already transmits an other packet or because the medium is busy (if sending
   * with CCA is requested) or that a transmission was aborted due to a change
   * of the channel of the transceiver.
   */
  virtual void NotifyTxFailure(const char *msg);

  /**
   * Indicates a change of the CCA value.
   */
  virtual void NotifyCcaChange(bool value);



  // methods for sending with user-configured checkCCA value
  virtual bool Send (Ptr<Packet> packet, bool checkCCA, const Address& dest, uint16_t protocolNumber);
  virtual bool SendFrom (Ptr<Packet> packet, bool checkCCA, const Address& source, const Address& dest, uint16_t protocolNumber);



  // inherited from NetDevice base class.
  virtual void SetIfIndex (const uint32_t index);
  virtual void SetAddress (Address address);
  virtual bool SetMtu (const uint16_t mtu);
  virtual void SetNode (Ptr<Node> node);

  virtual void AddLinkChangeCallback (Callback<void> callback);
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
  virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);

  virtual uint32_t GetIfIndex (void) const;
  virtual uint16_t GetMtu (void) const;

  virtual Ptr<Channel> GetChannel (void) const;
  virtual Ptr<Node> GetNode (void) const;

  virtual Address GetAddress (void) const;
  virtual Address GetBroadcast (void) const;
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;
  virtual Address GetMulticast (Ipv6Address addr) const;

  virtual bool IsLinkUp (void) const;
  virtual bool IsBroadcast (void) const;
  virtual bool IsMulticast (void) const;
  virtual bool IsPointToPoint (void) const;
  virtual bool IsBridge (void) const;
  virtual bool NeedsArp (void) const;
  virtual bool SupportsSendFrom (void) const;

  // the methods Send and SendFrom inherited from NetDevice base class
  // implicitly send with CCA, i.e. the sending fails if medium is busy
  virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);


protected:
  virtual void DoDispose (void);

  // these attributes have to be protected, because they are used by the
  // inherited CC2420InterfaceNetDevice
  Ptr<ErrorModel> m_receiveErrorModel;
  /**
   * The trace source fired when the phy layer drops a packet it has received
   * due to the error model being active.  Although CC2420NetDevice doesn't
   * really have a Phy model, we choose this trace source name for alignment
   * with other trace sources.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyRxDropTrace;

  Ptr<CC2420Phy> m_phy;

private:
  NetDevice::ReceiveCallback m_rxCallback;
  NetDevice::PromiscReceiveCallback m_promiscCallback;
  Ptr<Node> m_node;
  uint16_t m_mtu;
  uint32_t m_ifIndex;
  Mac48Address m_address;
  Ptr<ListErrorModel> m_listErrorModel;
};

} // namespace ns3

#endif /* CC2420_NET_DEVICE_H */
