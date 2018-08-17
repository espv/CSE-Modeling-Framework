#ifndef INTERRUPTCONTROLLER_H
#define INTERRUPTCONTROLLER_H

#include "ns3/object.h"
#include "ns3/packet.h"

#include <vector>
#include <stack>
#include <map>
#include <string>
#include <queue>
#include "thread.h"
#include "sem.h"


namespace ns3 {

#define NUMBER_OF_INTERRUPTS 255
#define DEFAULT_QUEUE_SIZE 255

class HWModel;
class Packet;
class EventImpl;
struct tempVar;

class InterruptRequest {
 public:
  // For interrupts with processing
  int interruptNr;
  Ptr<SEM> service;
  std::string serviceString;
  Ptr<Packet> current;
  struct tempVar tempsynch;
  std::map<std::string, Ptr<StateVariable> > localStateVariables;
  std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues;

  // For interrupts without processing
  EventImpl *toCall;
};

class InterruptController : public Object
{
public:
  static TypeId GetTypeId (void);

  InterruptController();
  InterruptController (int queue_size);
  virtual ~InterruptController () {};

  void IssueInterrupt(int number, std::string service, Ptr<Packet> current);
  virtual void IssueInterruptNoProcessing(int interruptNumber, EventImpl *callback);
  virtual void IssueInterruptWithService(Ptr<SEM> intSem, struct tempVar tempsynch, Ptr<Packet> current,
		  	  std::map<std::string, Ptr<StateVariable> > localStateVariables,
		  	  std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues);

  virtual void IssueInterruptWithServiceOnCPU(int cpu, Ptr<SEM> intSem, Ptr<ProgramLocation> programLoc);

  virtual void Proceed(int cpu);

  // If the SchedSim uses interrupts for a timer, we must
  // treat these specially here. These do not (for now)
  // consume any processing time.

  Ptr<HWModel> hwModel;

  bool masked[NUMBER_OF_INTERRUPTS];

private:
  bool in_progress[NUMBER_OF_INTERRUPTS];

  unsigned int queueSize;

  bool pending[NUMBER_OF_INTERRUPTS];
  std::queue<InterruptRequest> pendingRequests[10];
  InterruptRequest currentlyHandled[10];
};

} // namespace ns3

#endif /* INTERRUPTCONTROLLER_H */
