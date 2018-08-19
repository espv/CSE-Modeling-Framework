#ifndef CC2420_FSM_H
#define CC2420_FSM_H

#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ns3/timer.h"
#include "ns3/object.h"
#include "ns3/mac48-address.h"

namespace ns3
{
  class CC2420Phy;

  /**
   * Stores a packet and its accordant metadata. Used by the FSM to store the
   * currently transmitted, currently received and currently colliding packet.
   */
  class PacketMetaData : public ns3::Object {
  public:
    static TypeId GetTypeId (void);

    PacketMetaData();
    virtual ~PacketMetaData();
    void initialize(Ptr<Packet> packet, double powerDbm, Time startingTime);

    Ptr<Packet> packet;

    // tx or rx power (depends on whether this object is used to store
    // a sent or received packet)
    double powerDbm;

    Time startingTime;
  };

   /**
    * \brief The Finale State Machine of the CC2420-PHY-Layer.
	*	
	* This FSM gets request from the overlying CC2420Phy instance.
	* It determines the behavior of the PHY-Layer.
	*
	* Each time a packet is received from the channel the CC2420Phy instance
	* a rxFromMedium request is send. Depending on the current state of FSM
	* and following actions, the packet could be received after rxTime and given
	* back to the CC2420Phy, so it could send it up to the NetDevice-Layer.
	*
	* When a packet should be send, a txRequest is send to the FSM.
	* Depending on the regard of CCA, everytime the FSM give is ok
	* to CC2420Phy, so it can send the packet down to the channel,
	* or only when the medium looks free.
	* 
	*/
  class CC2420FSM : public ns3::Object
  {
  public:
    /**
     * The state of the PHY Final State Machine.
     */
    enum State
    {
      RXCAL=0,         // RX mode, receiver is still calibrating, air is idle
      RXCALNOISE=1,    // RX mode, receiver is still calibrating, air is busy
      RX=2,            // RX mode, CCA is invalid (first 8 symbols in RX mode), air is idle
      RXCCA=3,         // RX mode, CCA is valid, air is idle
      RXHDR=4,         // RX mode, receiving packet header (preamble, SFD)
      RXHDRCAP=5,      // RX mode, receiving packet header, 2nd packet is too weak to collide but will affect CCA
      RXDATA=6,        // RX mode, received header, receiving data
      RXDATACAP=7,     // RX mode, receiving data, 2nd packet in air is too weak to collide but will affect CCA
      RXCOLL=8,        // RX mode, received header, packet body is destroyed by a collission
      RXNOISE=9,       // RX mode, receiving noise (crippled header), CCA is invalid
      RXNOISECCA=10,   // RX mode, receiving noise, valid CCA
      TXCAL=11,        // TX mode, calibrate transmitter, medium is idle
      TXCALNOISE=12,   // TX mode, calibrate transmitter, medium is busy
      TX=13,           // TX mode, transmitting, no colliding packet
      TXNOISE=14       // TX mode, transmitting, colliding packet
    };

    static TypeId GetTypeId (void);

    CC2420FSM();

    virtual
    ~CC2420FSM();

    void SetPhy(Ptr<CC2420Phy> phy);
    void reset(bool switch_channel);

    // *** Simulation of CC2420
    void rxFromMedium(Ptr<Packet> packet, double rxPowerDbm);
    bool txRequest(Ptr<Packet> packet, double txPowerDbm, bool checkCCA);


  private:
  //protected:
    void collision(Ptr<PacketMetaData> packetMeta);
    void capture(Ptr<PacketMetaData> packetMeta);
    void delayCCASignal(bool cca);

    // *** Generic FSM functions
    void enterState(State newState);

    // *** Simulation of CC2420
    virtual void stateTimerExpired(void);
    virtual void rxTimerExpired(void);
    virtual void txTimerExpired(void);
    virtual void collTimerExpired(void);
    virtual void ccaTimerExpired(void);

  //private:
    Ptr<PacketMetaData> getLongerPacket(Ptr<PacketMetaData> packetMeta1, Ptr<PacketMetaData> packetMeta2); //nutzt remainingTime
    Time remainingTime(Ptr<PacketMetaData> packetMeta); //nutzt pktDuration
    Time pktDuration(Ptr<PacketMetaData> packetMeta);

    // *** Collision handling functions
    bool isCollision(Ptr<PacketMetaData> rxMeta, Ptr<PacketMetaData> collMeta);

    Time m_TSYM;
    Time m_THEADER;
    Time m_TwelveSymbolTime;
    Time m_EightSymbolTime;
    Ptr<CC2420Phy> m_phy;

    // *** Variables
    enum CC2420FSM::State m_fsmState; // State of the FSM representing the CC2420

    // *** Timers
    Timer m_stateTimer; // Used for changing states depending on time
    Timer m_rxTimer;    // Notifies when the air goes idle again
    Timer m_txTimer;    // Tracks the time required for transmitting packets
    Timer m_collTimer;  // Tracks packets that do not affect receiving but CCA
    Timer m_ccaTimer;   //  Timer delays the creation of a CCA event

    // *** more cc2420 related fields
    Ptr<PacketMetaData> m_rxPkt;      // Currently received packet
    Ptr<PacketMetaData> m_txPkt;      // Currently transmitted packet
    Ptr<PacketMetaData> m_collPkt;    // Holds packets that collide with the original packet - whether they are strong enough to collide or not

  };
} /* namespace ns3 */
#endif /* CC2420_FSM_H */
