#include "cc2420-fsm.h"
#include "cc2420-phy.h"
#include "cripple-tag.h"

#include "ns3/log.h"
#include <math.h>

NS_LOG_COMPONENT_DEFINE ("CC2420FSM");

namespace ns3
{

  CC2420FSM::CC2420FSM(){
	m_TSYM = MicroSeconds(16); // Symbol duration (16 us)
	// if the symbol duration is calculated from the phy transmission rate, this
	// leads to rounding errors; therefore, a direct value is used here

	//m_THEADER = MicroSeconds(160);

	// m_THEADER can only be calculated after the phy layer for the FSM has been set

	//m_THEADER = MicroSeconds(160); // CC2420 Header duration (160us = 40 Bit = 10 Sym)
    // the length of the header consists of the preamble length
    // (preambleLength + 1 bytes), the syncWord (2 bytes) and the frame length
    // (1 byte); each byte has 2 symbols
    //m_THEADER = MicroSeconds(
    //		(m_phy->GetPreambleLength() + 1 + 2 + 1) * (2 * m_TSYM));

    m_TwelveSymbolTime = MicroSeconds(m_TSYM.GetMicroSeconds() * 12);
    m_EightSymbolTime = MicroSeconds(m_TSYM.GetMicroSeconds() * 8);

    m_rxPkt = 0;
    m_txPkt = 0;
    m_collPkt = 0;
    m_stateTimer.SetFunction(&CC2420FSM::stateTimerExpired, this);
    m_rxTimer.SetFunction(&CC2420FSM::rxTimerExpired, this);
    m_txTimer.SetFunction(&CC2420FSM::txTimerExpired, this);
    m_collTimer.SetFunction(&CC2420FSM::collTimerExpired, this);
    m_ccaTimer.SetFunction(&CC2420FSM::ccaTimerExpired, this);
    // Start in RX state (this is done by SEnF on MicaZ)
    enterState (RXCAL);
  }

  CC2420FSM::~CC2420FSM()
  {
    m_phy = 0;
    m_rxPkt = 0;
    m_txPkt = 0;
    m_collPkt = 0;
    m_stateTimer.~Timer();
    m_rxTimer.~Timer();
    m_txTimer.~Timer();
    m_collTimer.~Timer();
    m_ccaTimer.~Timer();
  }

  TypeId
  CC2420FSM::GetTypeId(void)
  {
    static TypeId tid =
        TypeId("ns3::CC2420FSM").SetParent<Object>().AddConstructor<CC2420FSM>();
    return tid;
  }


  void
  CC2420FSM::SetPhy(Ptr<CC2420Phy> phy)
  {
	  m_phy = phy;

	  // Header duration
	  // the length of the header consists of the preamble length
      // (preambleLength + 1 bytes), the syncWord (2 bytes) and the frame length
      // (1 byte); each byte has 2 symbols
      m_THEADER = MicroSeconds((m_phy->GetPreambleLength() + 1 + 2 + 1) * 2
    		  * m_TSYM.GetMicroSeconds());
  }


  void
  CC2420FSM::enterState(State newState)
  {
    m_fsmState = newState;
    // Implement the actions on entering the states
    switch (m_fsmState)
      {
    case TXCAL:
      // Send the sending signal up to indicate the beginning of the transmission
      m_phy->GetDevice()->NotifyTxStart();

      // - Possibility to enter from TXCALNOISE, so start m_stateTimer only if
      //   if is not running yet (otherwise, the calibration has already begun)
      if (!m_stateTimer.IsRunning())
        {
    	  // Move to TX state after 12 or 8 symbols (depends on txTurnAround)
          m_stateTimer.Schedule(m_phy->GetTxTurnaround() ?
        		  m_TwelveSymbolTime : m_EightSymbolTime);
        }
      break;

    case RXCAL:
      // - Possibility to enter from RXCALNOISE, so start m_stateTimer only if
      //   it is not running yet (otherwise, the calibration has already begun)
      if (!m_stateTimer.IsRunning())
        {
          // Move to RX state after 12 symbols
          m_stateTimer.Schedule(m_TwelveSymbolTime);
        }
      break;

    case TXCALNOISE:
      // Send the sending signal up to indicate the beginning of the transmission
      m_phy->GetDevice()->NotifyTxStart();

      // - Possibility to enter from TXCAL, so start m_stateTimer only if
      //   it is not running yet (otherwise, the calibration has already begun)
      if (!m_stateTimer.IsRunning())
        {
          // Move to RX state after 12 symbols
          m_stateTimer.Schedule(m_phy->GetTxTurnaround() ?
        		  m_TwelveSymbolTime : m_EightSymbolTime);
        }
      break;

    case RXCALNOISE:
      // A packet arrived while calibrating receiver, do nothing here

      // - Possibility to enter from RXCAL, so start m_stateTimer only if
      //   it is not running yet (otherwise, the calibration has already begun)
      if (!m_stateTimer.IsRunning())
        {
          // Move to RX state after 12 symbols
          m_stateTimer.Schedule(m_TwelveSymbolTime);
        }
      break;

    case RX:
      // Reached rx state
      // - CCA pin will be valid after 8 symbol periods
      // - The medium is currently idle
      //if (m_stateTimer.IsRunning())
      //  m_stateTimer.Cancel();

      // Start the timer only if it's not busy - otherwise we changed from
      // RXNOISE to this state - the state timer is already set in this case
      if(!m_stateTimer.IsRunning())
    	  m_stateTimer.Schedule(m_EightSymbolTime);
      break;

    case RXCCA:
      // The CCA pin is valid by now, the medium is idle
      // - Send (maybe delay) a CCA(1) signal to the upper layers, indicating that
      //   the medium is idle
      delayCCASignal(true);
      break;

    case RXHDR:
      // Change state after the header has been received correctly
      // possibility to enter from RXHDRCAP (after second packet has been
      // finished), so set timer only if it is not running yet
      if(!m_stateTimer.IsRunning())
    	  m_stateTimer.Schedule(m_THEADER);
      // Send a CCA(0) signal to the upper layers, indicating that
      //   the medium is busy
      delayCCASignal(false);
      break;

    case RXHDRCAP:
      // State timer is already running from RXHDR, so do nothing here
      break;

    case RXNOISE:
      // Reached rx state
      // - The medium is currently busy
      // - CCA pin will be valid after 8 symbol periods, so we currently
      //   do not know that the medium is busy
      if (!m_stateTimer.IsRunning())
        {
          // Start the timer only if its not busy - otherwise we changed from
          // rx to this state - the state timer is already set in this case
          m_stateTimer.Schedule(m_EightSymbolTime); // Valid CCA pin after 8 symbols
        }
      break;

    case RXNOISECCA:
      // Possibility to enter from RXHDR, RXHDRCAP and
      // RXCCA (when signal is clearly noise)
      
      // - Stop m_stateTimer in this case
      if (m_stateTimer.IsRunning())
    	  m_stateTimer.Cancel();
      // The CCA pin is valid by now and the medium is busy
      // - Since we missed the header, only noise is seen
      // - Send a CCA(0) signal to the upper layers, indicating that
      //   the medium is busy
      delayCCASignal(false);
      break;

    case RXCOLL:
    case RXDATA:
    case RXDATACAP:
      // Do nothing here
      break;

    case TX:
    case TXNOISE:
      // Set the tx timer that will notify us if the transmission is complete
      // - maybe we get here from TX or TXNOISE, so set the timer only if its not already running
      if ((!m_txTimer.IsRunning()) && (m_txPkt != 0))
        {
          m_txTimer.Schedule(pktDuration(m_txPkt));
        }

      // Start the transmission - afterwards m_txPkt will be invalid
      // - Since they state may be entered multiple times, ensure that every packet
      //   is transmitted only once.
      if (m_txPkt != 0)
        m_phy->SendToChannel(m_txPkt->packet, m_txPkt->powerDbm);
      m_txPkt = 0;
      break;

    default:
      NS_FATAL_ERROR("Entered unknown state: " << m_fsmState << "\n");
      return;
      }
  }

  void
  CC2420FSM::txTimerExpired()
  {
    // Implement the possible actions on entering the states
    switch (m_fsmState)
      {
    case TX:
      // Send the SFD(1) signal up to indicate the end of the TX operation
      //m_phy->GetDevice()->SetSFDPin(true);

      //signal up to indicate the end of the TX operation
      m_phy->GetDevice()->NotifyTxEnd();

      // Finished transmission - enter RXCAL mode
      enterState (RXCAL);
      break;

    case TXNOISE:
      // Send the SFD(1) signal up to indicate the end of the TX operation
      //m_phy->GetDevice()->SetSFDPin(true);

      //signal up to indicate the end of the TX operation
      m_phy->GetDevice()->NotifyTxEnd();

      // Finished transmission - enter RXCALNOISE mode
      enterState (RXCALNOISE);
      break;

    case TXCAL:
    case TXCALNOISE:
    case RXCALNOISE:
    case RXNOISE:
    case RXHDRCAP:
    case RXNOISECCA:
    case RXDATACAP:
    case RXCOLL:
    case RX:
    case RXCAL:
    case RXHDR:
    case RXCCA:
    case RXDATA:
      NS_FATAL_ERROR(
          "m_txTimer timer in invalid state: " << m_fsmState << "\n");
      return;

    default:
      NS_FATAL_ERROR(
          "m_txTimer expired in unknown state: " << m_fsmState << "\n");
      return;
      }
  }

  void
  CC2420FSM::rxTimerExpired()
  {
    // Implement the possible actions on entering the states
    switch (m_fsmState)
      {
    case RXDATA:
      // Send received packet up
      m_phy->ForwardToDevice(m_rxPkt->packet, m_rxPkt->powerDbm);
      enterState (RXCCA);
      break;

    case RXDATACAP:
    case RXCOLL:
      // Send received packet up (eventually crippled),
      // create a wrapper packet etc.
      m_phy->ForwardToDevice(m_rxPkt->packet, m_rxPkt->powerDbm);

      if(m_collTimer.IsRunning())
    	  // there is still noise on the medium
    	  enterState (RXNOISECCA);
      else
    	  // collTimer has already expired, i.e. the colliding packet is
    	  // already finished and the medium is free

    	  // this can only happen in state RXCOLL, since in state RXDATACAP, the state
    	  // changes to RXDATA if the collision timer expires
    	  enterState(RXCCA);
      break;

    case RXHDR:
    case RXHDRCAP:
      NS_FATAL_ERROR("Packet smaller than its header!\n" << m_fsmState);
      break;

    case TX:
    case TXNOISE:
    case TXCAL:
    case TXCALNOISE:
    case RXCAL:
    case RXCALNOISE:
    case RXNOISE:
    case RXNOISECCA:
    case RX:
    case RXCCA:
      NS_FATAL_ERROR(
          "m_rxTimer timer in invalid state: " << m_fsmState << "\n");
      return;

    default:
      NS_FATAL_ERROR(
          "m_rxTimer expired in unknown/unexpected state: " << m_fsmState
              << "\n");
      return;
      }
  }

  void
  CC2420FSM::collTimerExpired()
  {
    // Implement the possible actions on entering the states
    switch (m_fsmState)
      {
    case RXCALNOISE:
      enterState (RXCAL);
      break;

    case RXNOISE:
      enterState (RX);
      break;

    case RXHDRCAP:
      enterState (RXHDR);
      break;

    case RXNOISECCA:
      enterState (RXCCA);
      break;

    case RXDATACAP:
      enterState (RXDATA);
      break;

    case RXCOLL:
    	// if collTimer fires in this state, the rxTimer is still running
    	// -> stay in RXCOLL
      //enterState(RX);
      break;

    case TXNOISE:
      // TXMode: CCA state is currently not visible
      enterState (TX);
      break;

    case TXCALNOISE:
      // TXMode: CCA state is currently not visible
      enterState (TXCAL);
      break;

    case TX:
    case TXCAL:
    case RX:
    case RXCAL:
    case RXHDR:
    case RXCCA:
    case RXDATA:
      NS_FATAL_ERROR(
          "m_collTimer expired in invalid state: " << m_fsmState << "\n");
      return;

    default:
      NS_FATAL_ERROR(
          "m_collTimer expired in unknown/unexpected state: " << m_fsmState
              << "\n");
      return;
      }
  }

  void
  CC2420FSM::stateTimerExpired()
  {
    //NS_LOG_UNCOND(m_fsmState << "stateTimerExpired: "<<Simulator::Now()<<"\n");
    switch (m_fsmState)
      {
    case RXCAL:
      // The transceiver reached rx state
      enterState (RX);
      break;

    case RXCALNOISE:
      // The transceiver reached rx state and a packet is on the medium
      // - Missed the packet header - move to RXNOISE
      // - CCA pin is not valid in RXNOISE, so the upper layers will be
      //   notified when entering state RXNOISECCA
      enterState (RXNOISE);
      break;

    case RX:
      // 8 symbol periods passed in rx state - the CCA pin will be valid
      // by now
      enterState (RXCCA);
      break;

    case RXHDR:
      // Sucessfully received header - start receiving the data part
      enterState (RXDATA);
      break;

    case RXHDRCAP:
      // Sucessfully received header - start receiving the data part
      enterState (RXDATACAP);
      break;

    case RXNOISE:
      // 8 symbol periods passed in rx state - the CCA pin will be valid
      // by now
      enterState (RXNOISECCA);
      break;

    case TXCALNOISE:
      // Start transmision
      enterState (TXNOISE);
      break;

    case TXCAL:
      // Start transmision
      enterState (TX);
      break;

    case TXNOISE:
    case TX:
    case RXNOISECCA:
    case RXCCA:
    case RXDATA:
    case RXDATACAP:
    case RXCOLL:
      NS_FATAL_ERROR("State timer in invalid state: " << m_fsmState << " \n");
      return;

    default:
      NS_FATAL_ERROR("State timer in unknown state: " << m_fsmState << " \n");
      return;
      }
  }

  void
  CC2420FSM::ccaTimerExpired()
  {
    switch (m_fsmState)
      {
    case RXCALNOISE:
    case RXHDR:
    case RXHDRCAP:
    case RXDATA:
    case RXDATACAP:
    case RXNOISECCA:
    case RXCOLL:
      // Create CCA signal (medium is now busy) that is sent up
      //NS_LOG_FUNCTION(m_fsmState<<Simulator::Now().GetSeconds()<<" : ccaTimerExpired SENDING CCA = false\n");
      m_phy->GetDevice()->NotifyCcaChange(false);
      break;

    case RXCAL:
    case RXCCA:
      // Create CCA signal (medium is now idle) that is sent up
      //NS_LOG_FUNCTION(m_fsmState<<Simulator::Now().GetSeconds()<<" : ccaTimerExpired SENDING CCA = true\n");
      m_phy->GetDevice()->NotifyCcaChange(true);
      break;

    case TXCALNOISE:
    case TXCAL:
      //NS_LOG_FUNCTION(m_fsmState<<Simulator::Now().GetSeconds()<<" : ccaTimerExpired staying silent\n");
      // Do nothing: Medium became busy just before changing to TX mode
      // --> Node does not detect the "busy medium", i.e. no CCA signal
      break;

    case RX:
    case RXNOISE:
    case TXNOISE:
    case TX:
      NS_FATAL_ERROR(
          "" << Simulator::Now().GetSeconds()
              << " : ccaTimerExpired, CCA timer in invalid state: "
              << m_fsmState << "\n");
      return;

    default:
      NS_FATAL_ERROR(
          "" << Simulator::Now().GetSeconds()
              << " : ccaTimerExpired, CCA timer in unknown state: "
              << m_fsmState << "\n");
      return;
      }
  }

  
  void
  CC2420FSM::capture(Ptr<PacketMetaData> packetMeta)
  {
    // Original packet is strong enough - no collision
    // Store the colliding packet for correct CCA simulation
    if ((m_fsmState == RXDATACAP) || (m_fsmState == RXHDRCAP))
      {
        // Determine the longer packet if there is already a packet
        // on the medium
        m_collPkt = getLongerPacket(m_collPkt, packetMeta);
      }
    else
      {
        // No other packet on the medium - store packet and
        // the current time
        m_collPkt = packetMeta;
      }
    // Set coll timer to the longer packet
    if (m_collTimer.IsRunning())
      m_collTimer.Cancel();

    m_collTimer.Schedule(remainingTime(m_collPkt));
  }

  void
  CC2420FSM::rxFromMedium(Ptr<Packet> packet, double rxPowerDbm)
  {
    Ptr < PacketMetaData > currentPacketMeta = CreateObject<PacketMetaData>();
    currentPacketMeta->initialize(packet, rxPowerDbm, Simulator::Now());

    CrippleTag cripple;

    // Implement the possible actions on entering the states
    switch (m_fsmState)
      {
    case RXCAL:
      // Start the packet receive timer that will notify when the
      // air goes idle again
      m_collTimer.Schedule(pktDuration(currentPacketMeta));
      // Store the currently received packet
      m_collPkt = currentPacketMeta;
      // A packet arrives while we are calibrating the receiver
      // - The packet will only be seen as random noise
      // - The CCA pin will go high in state RXNOISECCA
      enterState (RXCALNOISE);
      break;

    case RX:
      // Start the packet receive timer that will notify when the
      // air goes idle again
      m_collTimer.Schedule(pktDuration(currentPacketMeta));
      // Store the currently received packet
      m_collPkt = currentPacketMeta;
      // A packet arrives while we are not yet ready to handle it
      // - The packet will only be seen as random noise because the header is missing
      // - The CCA pin will go high in state RXNOISECCA
      enterState (RXNOISE);
      break;

    case RXCCA:
      //check if packet is crippled/flagged as Noise
      if (currentPacketMeta->packet->RemovePacketTag(cripple)){
        //signal can not correctly received (other SyncWord or Header unheard)
        //handle signal as noise for CCA
        
        // Store the colliding packet for correct CCA simulation
        capture (currentPacketMeta);
        // Enter RXNOISECCA state (CCA is set there)
        enterState (RXNOISECCA);
      } else {
        // Start the packet receive timer that will notify when the
        // air goes idle again
        m_rxTimer.Schedule(pktDuration(currentPacketMeta));
        // Store the currently received packet
        m_rxPkt = currentPacketMeta;
        // Enter RXHDR state
        // - Start receiving the header
        // - The CCA pin will go high in RXHDR
        enterState (RXHDR);
      }
      break;

    case RXHDR:
    case RXHDRCAP:
      // Another packet arrives while the air is already busy
      // Check if the new packet is strong enough to collide
      if (isCollision(m_rxPkt, currentPacketMeta))
        {
          // Collision occurred - the rx packet will become noise by now
          collision (currentPacketMeta);
          // The m_rxPkt does not exist anymore, so don't care about simulating
          // errors at this time
          m_rxPkt = 0;
          // Stop rx timer - did not receive a valid header, so no packet is
          // visible
          m_rxTimer.Cancel();
          // Enter RXNOISECCA state
          enterState (RXNOISECCA);
        }
      else
        {
          // Original packet is strong enough - no collision
          // Store the colliding packet for correct CCA simulation
          capture (currentPacketMeta);
          // Enter RXHDRCAP state (only if not already there)
          if (m_fsmState != RXHDRCAP)
            enterState (RXHDRCAP);
        }
      break;

    case RXDATA:
    case RXDATACAP:
      // Another packet arrives while the air is already busy
      // Check if the new packet is strong enough to collide
      if (isCollision(m_rxPkt, currentPacketMeta))
        {
          // collision while receiving packet body - packet will become crippled
          collision (currentPacketMeta);
          // Set error flag of the packet
          std::list < uint32_t > errorList =
              m_phy->GetDevice()->GetPacketErrorList();
          errorList.push_back(m_rxPkt->packet->GetUid());
          m_phy->GetDevice()->SetPacketErrorList(errorList);
          // Enter RXCOLL state
          enterState (RXCOLL);
        }
      else
        {
          // Original packet is strong enough - no collision
          // Store the colliding packet for correct CCA simulation
          capture (currentPacketMeta);
          // Enter RXHDRCAP state (only if not already there)
          if (m_fsmState != RXDATACAP)
            enterState (RXDATACAP);
        }
      break;

    case RXCOLL:
    case RXCALNOISE:
    case RXNOISE:
    case RXNOISECCA:
    case TXNOISE:
    case TXCALNOISE:
      // Another packet arrives while the air is already busy

      // The packet is crippled anyway, so we will just simulate CCA
      // Store the packet that will last longer, free the other one
      m_collPkt = getLongerPacket(m_collPkt, currentPacketMeta);
      // Adjust rx timer to the longer packet
      m_collTimer.Cancel();
      m_collTimer.Schedule(remainingTime(m_collPkt));

      break;

    case TXCAL:
      m_collPkt = currentPacketMeta;
      m_collTimer.Schedule(pktDuration(m_collPkt));
      enterState (TXCALNOISE);
      break;

    case TX:
      m_collPkt = currentPacketMeta;
      m_collTimer.Schedule(pktDuration(m_collPkt));
      enterState (TXNOISE);
      break;

    default:
      NS_FATAL_ERROR(
          "Received packet from medium in unknown/unexpected state: %i\n"
              << m_fsmState);
      return;
      }
  }

  bool
  CC2420FSM::txRequest(Ptr<Packet> packet, double txPowerDbm, bool checkCCA)
  {
    bool txSuccess = true;
    // Store packet to be transmitted
    m_txPkt = CreateObject<PacketMetaData>();
    m_txPkt->initialize(packet, txPowerDbm, Simulator::Now());
    // Implement the possible actions on entering the states
    switch (m_fsmState)
      {
    // TX request while transmitting --> send error signal
    case TXCAL:
    case TX:
    case TXCALNOISE:
    case TXNOISE:
      // Send the SFD(0) signal up to indicate the failure of the TX operation

      m_phy->GetDevice()->NotifyTxFailure("Already transmitting another packet.");
      txSuccess = false;
      break;

      // TX request is possible in all rx states
    case RXCAL:
    case RX:
    case RXCCA:
      // stop state timer, since tx calibration must start from the beginning
      if (m_stateTimer.IsRunning())
    	  m_stateTimer.Cancel();

      // medium is idle
      enterState (TXCAL);
      break;

    case RXHDR:
    case RXHDRCAP:
    case RXDATA:
    case RXDATACAP:
	case RXCOLL:
		// Do not start transmitting if the medium is not idle and
		// carrier sensing before sending is requested
		if (checkCCA) {
			// Send the SFD(0) signal up to indicate the failure of the TX
			// operation

			m_phy->GetDevice()->NotifyTxFailure("Medium is busy (CCA).");
			txSuccess = false;
			break;
		}

		// RXData will become noise
		if ((m_fsmState == RXHDR) || (m_fsmState == RXDATA)) {
			m_collPkt = m_rxPkt;
		}

		// The longer packet will become noise and will be tracked
		if ((m_fsmState == RXHDRCAP) || (m_fsmState == RXDATACAP)
				|| (m_fsmState == RXCOLL)) {
			// The packet is crippled anyway, so we will just simulate CCA
			Ptr<PacketMetaData> longerPkt = getLongerPacket(m_collPkt, m_rxPkt);
			// Store the packet that will last longer, free the other one
			if (longerPkt == m_collPkt) {
				m_rxPkt = 0;
			} else {
				m_collPkt = 0;
				m_collPkt = m_rxPkt;
			}
		}

		//if ((m_fsmState == RXHDR) || (m_fsmState == RXDATA)
		//		|| (m_fsmState == RXHDRCAP) || (m_fsmState == RXDATACAP)) {

		// Stop rx timer (rx packet is noise by now)
		if (m_rxTimer.IsRunning())
			m_rxTimer.Cancel();

		//}

		// Adjust collision timer to the longer packet
		if (m_collTimer.IsRunning())
			m_collTimer.Cancel();
		m_collTimer.Schedule(remainingTime(m_collPkt));

		// stop state timer, since tx calibration must start from the beginning
		if (m_stateTimer.IsRunning())
			m_stateTimer.Cancel();

		enterState(TXCALNOISE);
		break;

    case RXNOISE:
    case RXCALNOISE:
    case RXNOISECCA:
      // Do not start transmitting if the medium is not idle and
      // carrier sensing before sending is requested
      if (checkCCA == true)
        {
          // Send the SFD(0) signal up to indicate the failure of the TX
          // operation

          m_phy->GetDevice()->NotifyTxFailure("Medium is busy (CCA).");
          txSuccess = false;
          break;
          //no carrier sensing
        }

      // Coll timer will keep running
      // stop state timer, since tx calibration must start from the beginning
      if (m_stateTimer.IsRunning())
    	  m_stateTimer.Cancel();

      enterState(TXCALNOISE);
      break;

    default:
      NS_FATAL_ERROR(
          "Received tx request in unknown state: " << m_fsmState << "\n");
      txSuccess = false;
      break;
      }
    return txSuccess;
  }

  /* Simulate a collision while receiving the header of a packet.
   * - pkt is invalid after calling this function
   * - collPkt and collStart is updated to the packet that will last longest
   */
  void
  CC2420FSM::collision(Ptr<PacketMetaData> packetMeta)
  {
    // collision occurred
    // Calculate the packet that will last the longest time
    Ptr < PacketMetaData > tmpCollPacket = getLongerPacket(m_rxPkt, packetMeta);
    // Is there a third packet on the medium ?
    if ((m_fsmState == RXDATACAP) || (m_fsmState == RXHDRCAP))
      {
        // Calculate the packet that will last the longest time
        m_collPkt = getLongerPacket(m_collPkt, tmpCollPacket);
      }
    else
      {
        // No third packet
        m_collPkt = tmpCollPacket;
      }
    // Set coll timer to the longer packet
    if (m_collTimer.IsRunning())
      m_collTimer.Cancel();

    m_collTimer.Schedule(remainingTime(m_collPkt));
  }
  
  Ptr<PacketMetaData>
  CC2420FSM::getLongerPacket(Ptr<PacketMetaData> packetMeta1,
      Ptr<PacketMetaData> packetMeta2)
  {
    if (remainingTime(packetMeta1) > remainingTime(packetMeta2))
      {
        return packetMeta1;
      }
    else
      {
        return packetMeta2;
      }
  }

  Time
  CC2420FSM::pktDuration(Ptr<PacketMetaData> packetMeta)
  {
    double transmissionTime;
    uint32_t size = packetMeta->packet->GetSize();
    uint32_t rate = m_phy->GetTransmissionRateBytesPerSecond();
    transmissionTime = double(size) / double(rate);
    //If Time less than zero -> then abort

    return Seconds(transmissionTime);
  }

  void
  CC2420FSM::reset(bool switch_channel)
  {
    if(m_txTimer.IsRunning()){
        m_phy->GetDevice()->NotifyTxFailure("Aborting Transmission and switching channel.");
    }
    m_txPkt = 0;

    m_txTimer.Cancel();
    m_ccaTimer.Cancel();
    m_stateTimer.Cancel();

    if(!switch_channel){
        if(m_rxTimer.IsRunning()){
            m_collPkt = getLongerPacket(m_rxPkt, m_collPkt);
            m_collTimer.Schedule(remainingTime(m_collPkt));
        }
    } else {
        m_collPkt = 0;
        m_collTimer.Cancel();
    }
    m_rxTimer.Cancel();
    m_rxPkt = 0;

    if(m_collTimer.IsRunning()){
        enterState(RXCALNOISE);
    } else {
        enterState(RXCAL);
    }
  }

  bool
  CC2420FSM::isCollision(Ptr<PacketMetaData> rxMeta,
      Ptr<PacketMetaData> collMeta)
  {
	// the packet is strong enough to be captured, if the signal strength
	// difference is at least 3 dBm
	return (rxMeta->powerDbm -3) < collMeta->powerDbm;


	// this does not work, since the signal strength values can be negative

    // Check if the packet is strong enough to be captured, if it is not strong
    // enough, then it will collide with the newly arrived packet.
    // - CPThresh is controlled by phy
	//return (rxMeta->powerDbm / collMeta->powerDbm)
	//       < m_phy->GetCaptureThresholdDb();
  }

  Time
  CC2420FSM::remainingTime(Ptr<PacketMetaData> packetMeta)
  {
    Time remTime = MicroSeconds(0);

    if(packetMeta){
        Time txTime = pktDuration(packetMeta);
        Time timePassed = Simulator::Now() - packetMeta->startingTime;
        remTime = txTime - timePassed;
    }
    return remTime;
  }

  /*****************************************************************************
   * Consider signal strength in order to (possibly) delay the CCA event
   * signaled to the upper layer
   *****************************************************************************/
  void
  CC2420FSM::delayCCASignal(bool cca)
  {
    if (m_phy->getMaxCCADelay() == 0.0 || m_rxPkt == 0)
      {
        // No delay - CCA packet is created at the start of frame reception
        m_phy->GetDevice()->NotifyCcaChange(cca);
        return;
      }

    // Delay CCA event. The delay depends on the min/max possible delay
    // and the current received signal strength
    if (!m_ccaTimer.IsRunning())
      {
        // Calculate delay of cca signal caused by weak signal energy
        //double thresholdCSStrength_dBm = m_phy->GetCSThresholdDb();
        double thresholdCSStrength_dBm = m_phy->GetCSThresholdDBm() - m_phy->GetCCAHyst();
        double currentCSStrength_dBm = m_rxPkt->powerDbm;
        double strength_fraction = (currentCSStrength_dBm
            / thresholdCSStrength_dBm);
        strength_fraction = strength_fraction * strength_fraction;
        // Fits signal behavior better
        if (strength_fraction > 1)
          {
            strength_fraction = 1;
          }
        else if (strength_fraction < 0)
          {
            strength_fraction = 0;
          }

        // Note: strength_fraction is now >=0 and <=1
        // If medium is now free, it is signalized faster if signal strength is low (less delay)
        //    -> invert signal strength
        if (cca == true)
          {
            strength_fraction = 1 - strength_fraction;
          }
        // Check borders
        Time addDelay =
            MicroSeconds(
                (m_phy->getMaxCCADelay() - m_phy->getMinCCADelay()).GetMicroSeconds()
                    * strength_fraction);
        if (addDelay <= 0)
          {
            //NS_LOG_UNCOND(m_fsmState << Simulator::Now().GetSeconds() << " : CCA=" << cca << " DELAY=" << addDelay.GetSeconds() <<" RECEIVE_AT=NOW\n");
            m_phy->GetDevice()->NotifyCcaChange(cca);
          }
        else
          {
            //NS_LOG_UNCOND(m_fsmState << Simulator::Now().GetSeconds() << " : CCA="<< cca <<" DELAY=" << addDelay <<" RECEIVE_AT=" << (Simulator::Now()+addDelay).GetSeconds());
            m_ccaTimer.Schedule(m_phy->getMinCCADelay() + addDelay);
          }
      }
  }

  TypeId
  PacketMetaData::GetTypeId(void)
  {
    static TypeId tid =
        TypeId("ns3::PacketMetaData").SetParent<Object>().AddConstructor<
            CC2420FSM>();
    return tid;
  }

  PacketMetaData::PacketMetaData()
  {
  }

  PacketMetaData::~PacketMetaData()
  {
    packet = 0;
  }

  void
  PacketMetaData::initialize(Ptr<Packet> packet, double powerDbm,
      Time startingTime)
  {
    this->packet = packet;
    this->powerDbm = powerDbm;
    this->startingTime = startingTime;
  }
/* namespace ns3 */

}

