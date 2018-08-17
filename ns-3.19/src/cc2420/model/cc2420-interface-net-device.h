#ifndef CC2420_INTERFACE_NET_DEVICE_H_
#define CC2420_INTERFACE_NET_DEVICE_H_

#include "ns3/cc2420-messages.h"
#include "cc2420-net-device.h"

namespace ns3
{

  class CC2420InterfaceNetDevice : public CC2420NetDevice
  {
	public:
      static TypeId GetTypeId (void);
      CC2420InterfaceNetDevice();

      void descendingSignal (Ptr<CC2420Message> msg);

      /**
       * Callback for receiving CC2420Messages which are created by this
       * NetDevice.
       */
      typedef Callback< bool,Ptr<CC2420Message> > MessageCallback;

      /**
       * \param cb callback to invoke whenever this NetDevice has a
       * 		CC2420Message for the higher layers.
       */
       virtual void SetMessageCallback (MessageCallback cb);


       /**
        * This method is called by the phy layer when a packet is received from
        * the channel. A CC2420Receive message is sent to the upper layers.
        * This method overrides the one in CC2420NetDevice.
        */
       virtual bool Receive (Ptr<Packet> packet, bool crc, double rxPowerDbm);

       /**
        * Indicates the beginning of a transmission, so a CC2420Sending(true)
        * message will be sent to the upper layers.
        * This method overrides the one in CC2420NetDevice.
        */
       virtual void NotifyTxStart(void);

       /**
        * Indicates the end of a transmission, so a CC2420SendFinished()
        * message will be sent to the upper layers.
        * This method overrides the one in CC2420NetDevice.
        */
       virtual void NotifyTxEnd(void);

       /**
        * Indicates that a transmission could not start or has been aborted,
        * so a CC2420Sending(false) message will be sent to the upper layers.
        * This method overrides the one in CC2420NetDevice.
        */
       virtual void NotifyTxFailure(const char *msg);

       /**
       * Indicates a change of the CCA value, so a CC2420Cca(value) message
       * will be sent to the upper layers.
       * This method overrides the one in CC2420NetDevice.
       */
       virtual void NotifyCcaChange(bool value);


   private:

      MessageCallback messageCb;
  };

} /* namespace ns3 */
#endif /* CC2420_INTERFACE_NET_DEVICE_H_ */
