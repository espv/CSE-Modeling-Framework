#ifndef PACKET_SINK_CC2420_H
#define PACKET_SINK_CC2420_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/cc2420-interface-net-device.h"

namespace ns3 {

class Packet;

/**
 * \ingroup applications 
 * \defgroup packetsinkcc2420 PacketSinkCC2420
 *
 * This application was written to complement OnOffApplicationCC2420.
 */

/**
 * \ingroup packetsinkcc2420
 *
 * \brief Receive and consume traffic generated to a CC2420InterfaceNetDevice
 */
class PacketSinkCC2420 : public Application
{
public:
  static TypeId GetTypeId (void);
  PacketSinkCC2420 ();

  virtual ~PacketSinkCC2420 ();

  /**
   * \return the total bytes received in this sink app
   */
  uint32_t GetTotalRx () const;

 
protected:
  virtual void DoDispose (void);
private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  // handle messages from the CC2420InterfaceNetDevice
  bool HandleRead (Ptr<CC2420Message> msg);

  uint32_t        m_totalRx;      // Total bytes received
  TypeId          m_tid;          // Protocol TypeId
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;

  // pointer to the CC2420InterfaceNetDevice which is used for sending and
  // receiving messages (the application uses only one such
  // NetDevice, which is the first one in the accordant nodes' NetDevice-list
  // and has to be of type CC2420InterfaceNetDevice)
  Ptr<CC2420InterfaceNetDevice> netDevice;

};

} // namespace ns3

#endif /* PACKET_SINK_CC2420_H */

