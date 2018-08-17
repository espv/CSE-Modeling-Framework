#include <ns3/queue.h>
#include "condition.h"
#include "ns3/callback.h"
#include "execenv.h"
#include "ns3/interrupt-controller.h"

#include <queue>

namespace ns3 {

uint32_t ConditionFunctions::PacketSize(Ptr<Thread> t) {
  return t->m_currentLocation->curPkt->GetSize();
}

uint32_t ConditionFunctions::PacketL4Protocol(Ptr<Thread> t) {
  // TODO
  return 0;
}

// Conditions working on queues and threads
uint32_t ConditionFunctions::QueueCondition(Ptr<Queue> first, Ptr<Queue> last)
{
	if(first == last)
		return first->IsEmpty();
	else {
		std::vector<Ptr<Queue> > *queueOrder = &node->GetObject<ExecEnv> ()->queueOrder;
		 std::vector<Ptr<Queue> >::iterator firstFound =
				 std::find(queueOrder->begin(), queueOrder->end(), first);
		 std::vector<Ptr<Queue> >::iterator lastFound =
				 std::find(queueOrder->begin(), queueOrder->end(), last);

		std::vector<Ptr<Queue> >::iterator it = queueOrder->begin();
		for(;firstFound != lastFound && it != queueOrder->end(); firstFound++)
			if(!((*firstFound)->IsEmpty()))
				return QUEUENOTEMPTY;
	}

	return QUEUEEMPTY;
}

uint32_t ConditionFunctions::ServiceQueueCondition(std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *first, std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *last)
{
	if(first == last)
		return first->empty();
	else {
		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *>  *serviceQueueOrder = &node->GetObject<ExecEnv> ()->serviceQueueOrder;
		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *> ::iterator firstFound =
				 std::find(serviceQueueOrder->begin(), serviceQueueOrder->end(), first);
		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *>::iterator lastFound =
				 std::find(serviceQueueOrder->begin(), serviceQueueOrder->end(), last);

		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *>::iterator it = serviceQueueOrder->begin();
		for(;firstFound != lastFound && it != serviceQueueOrder->end(); firstFound++)
		  if(!(*firstFound)->empty()) {
				return QUEUENOTEMPTY;
			}
	}

	return QUEUEEMPTY;
}
// Conditions working on threads
uint32_t ConditionFunctions::ThreadCondition(std::string threadId)
{
  return 0;
}


TypeId
ConditionFunctions::GetTypeId(void)
{
  static TypeId tid = TypeId ("ns3::processing::ConditionFunctions")
    .SetParent<Object> ()
    .AddConstructor<ConditionFunctions> ()
    ;
  return tid;  
}

ConditionFunctions::ConditionFunctions()
  : conditionMap()
{
	this->m_wl1251NICIntrReg = 0;
	this->numTxToAck = 0;
}

uint32_t ConditionFunctions::ReadNumTxToAck(Ptr<Thread> t) {
//	std::cout << "wl1251_tx_complete: INTR = " << m_wl1251NICIntrReg;

	// Clear tx complete bit
	if(m_wl1251NICIntrReg == 2) {
		m_wl1251NICIntrReg = 0;
	} else if(m_wl1251NICIntrReg == 3) {
		m_wl1251NICIntrReg = 1;
	} else if (m_wl1251NICIntrReg == 11) {
		m_wl1251NICIntrReg = 9;
	}

//	std::cout << " -> " << m_wl1251NICIntrReg << ", #toCompl = " << numTxToAck;

	// Fetch and clear number to ack
	uint32_t toReturn = numTxToAck;
	numTxToAck = 0;

//	std::cout << " -> " << numTxToAck << std::endl;

	return toReturn;
}

uint32_t
ConditionFunctions::BCM4329DataOk(Ptr<Thread> t) {
	return 1;
}

void
ConditionFunctions::WriteWl1251Intr(Ptr<Thread> t, uint32_t value) {
	m_wl1251NICIntrReg = value;
}

uint32_t
ConditionFunctions::Wl1251RxLoop(Ptr<Thread> t) {
	if(t->m_currentLocation->curIteration >= 10)
		return 0;

//node->GetObject<ExecEnv>()->globalStateVariables["wl1251:interrupttype"] = m_wl1251NICIntrReg;
	//uint32_t retVal = m_wl1251NICIntrReg;
	//m_wl1251NICIntrReg = 0;

	return node->GetObject<ExecEnv>()->globalStateVariables["wl1251:interrupttype"];
}

uint32_t
ConditionFunctions::ReadWl1251Intr(Ptr<Thread> t) {
	node->GetObject<ExecEnv>()->globalStateVariables["wl1251:interrupttype"] = m_wl1251NICIntrReg;
	uint32_t retVal = m_wl1251NICIntrReg;
	m_wl1251NICIntrReg = 0;

	return retVal;
}

uint32_t
ConditionFunctions::Wl1251Intr(Ptr<Thread> t) {
	return node->GetObject<ExecEnv>()->globalStateVariables["wl1251:interrupttype"];
}

uint32_t
ConditionFunctions::sizeofnextrxfromnic(Ptr<Thread> t) {
	Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();
	return ee->queues["nic::rx"]->Peek()->GetSize();
}

void ConditionFunctions::WriteInterruptsEnabled(Ptr<Thread> t, uint32_t value) {
	node->GetObject<ExecEnv>()->hwModel->m_interruptController->masked[202] = (value == 0);
}

void ConditionFunctions::AckNICRx(Ptr<Thread> t, uint32_t value) {
	// Set intr-register to correct value
	Ptr<ExecEnv> ee = t->peu->hwModel->node->GetObject<ExecEnv>();
	// Set intr-register to correct value
	if(ee->queues["nic::rx"]->GetNPackets() == 0) {
		if(m_wl1251NICIntrReg == 1) {
			m_wl1251NICIntrReg = 0;
		} else if (m_wl1251NICIntrReg == 3) {
			m_wl1251NICIntrReg = 2;
		} else if (m_wl1251NICIntrReg == 9) {
			m_wl1251NICIntrReg = 0;
		} else if (m_wl1251NICIntrReg == 11) {
			m_wl1251NICIntrReg = 2;
		}
	} else if(ee->queues["nic::rx"]->GetNPackets() == 1) {
		if(m_wl1251NICIntrReg == 0) {
			m_wl1251NICIntrReg = 1;
		} else if(m_wl1251NICIntrReg == 2) {
			m_wl1251NICIntrReg = 3;
		} else if (m_wl1251NICIntrReg == 11) {
			m_wl1251NICIntrReg = 3;
		} else if (m_wl1251NICIntrReg == 9) {
			m_wl1251NICIntrReg = 1;
		}
	} else if(ee->queues["nic::rx"]->GetNPackets() >= 2) {
		if(m_wl1251NICIntrReg == 0) {
			m_wl1251NICIntrReg = 9;
		} else if(m_wl1251NICIntrReg == 1) {
			m_wl1251NICIntrReg = 9;
		} else if (m_wl1251NICIntrReg == 2) {
			m_wl1251NICIntrReg = 11;
		} else if (m_wl1251NICIntrReg == 3) {
			m_wl1251NICIntrReg = 11;
		}
	}
}


void
ConditionFunctions::Initialize(Ptr<ExecEnv> execenv)
{
  // Set the processing object
  node = execenv->GetObject<Node> ();

  //conditionMap["send_info_empty"] = MakeCallback(&ConditionFunctions::IpSendInfoEmpty, this);
  //conditionMap["send_info_full"] = MakeCallback(&ConditionFunctions::IpSendInfoFull, this);
  //conditionMap["task_queue_empty"] = MakeCallback(&ConditionFunctions::TaskQueueEmpty, this);
  //conditionMap["send_packets_full"] = MakeCallback(&ConditionFunctions::IpPacketsFull, this);
  //conditionMap["send_packets_empty"] = MakeCallback(&ConditionFunctions::IpPacketsEmpty, this);

  // Set the protocol-agnostic condition functions
  /*conditionMap["pkt::ip::size"] = MakeCallback(&ConditionFunctions::PacketSize, this);
  conditionMap["bcm4329::dataok"] = MakeCallback(&ConditionFunctions::BCM4329DataOk, this);
  conditionMap["readStatewl1251:interrupttype"] = MakeCallback(&ConditionFunctions::Wl1251Intr, this);
  conditionMap["readStatewl1251:readinterrupttype"] = MakeCallback(&ConditionFunctions::ReadWl1251Intr, this);
  conditionMap["wl1251rxloop"] = MakeCallback(&ConditionFunctions::Wl1251RxLoop, this);
  conditionMap["readStatewl1251:sizeofnextrxfromnic"] = MakeCallback(&ConditionFunctions::sizeofnextrxfromnic, this);
  writeConditionMap["writeStatewl1251:interruptenabled"] = MakeCallback(&ConditionFunctions::WriteInterruptsEnabled, this);
  writeConditionMap["writeStateacknicrx"] = MakeCallback(&ConditionFunctions::AckNICRx, this);
  conditionMap["readStatewl1251:numtxcomplete"] = MakeCallback(&ConditionFunctions::ReadNumTxToAck, this);*/


  // Add later, when the function is finished:
  // conditionMap["pktl4protocol"] = MakeCallback(&ConditionFunctions::PacketL4Protocol, this);
}

}
