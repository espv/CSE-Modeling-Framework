#ifndef APIC_H
#define APIC_H

#include "interrupt-controller.h"
#include "taskscheduler.h"

namespace ns3 {

class APIC : public InterruptController
{
public:
  static TypeId GetTypeId (void);

  APIC();
  APIC(int queue_size);
  // virtual ~APIC () {};

  /*
  void IssueInterrupt(int number, std::string service, Ptr<Packet> current);
  void IssueInterruptNoProcessing(int interruptNumber, EventImpl *callback);
  void Proceed();
  */

  void IssueInterruptWithService(Ptr<SEM> intSem, struct tempVar tempsynch, Ptr<Packet> current,
		  	  std::map<std::string, Ptr<StateVariable> > localStateVariables,
		  	  std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues);

  void IssueInterruptWithServiceOnCPU(int cpu, Ptr<SEM> intSem, struct tempVar tempsynch, Ptr<Packet> current,
		  	  std::map<std::string, Ptr<StateVariable> > localStateVariables,
		  	  std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues);

  // If the SchedSim uses interrupts for a timer, we must
  // treat these specially here. These do not (for now)
  // consume any processing time.

  Ptr<HWModel> hwModel;

  // bool masked[NUM_CPU][NUMBER_OF_INTERRUPTS];

  bool masked[NUMBER_OF_INTERRUPTS];
private:
  bool pending[NUMBER_OF_INTERRUPTS];
  bool in_progress[NUMBER_OF_INTERRUPTS];

  unsigned int queueSize;

  std::queue<InterruptRequest> pendingRequests[NUM_CPU];
  InterruptRequest currentlyHandled[NUM_CPU];
};

} // namespace ns3

#endif
