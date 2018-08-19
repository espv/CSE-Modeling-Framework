#ifndef PROGRAM_H
#define PROGRAM_H

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable.h"
#include "ns3/event-id.h"
#include "ns3/queue.h"
//#include "ns3/execenv.h"
#include "ns3/condition.h"
#include "ns3/local-state-variable.h"
#include "ns3/local-state-variable-queue.h"

#include <vector>
#include <stack>
#include <map>
#include <queue>
#include <iostream>
#include <utility>

namespace ns3 {

class Thread;
class PEU;
class Condition;
class SEM;
class Packet;
class ProgramLocation;

// Currently used as index into vectors of resource consumption
// in ProcessingStage and ProcessingInstance classes.
enum ResourceType {
  NANOSECONDS,
  CYCLES,
  INSTRUCTIONS,
  CACHEMISSES,
  MEMORYACCESSES,
  MEMSTALLCYCLES,

  // Must be here to know for how long we should
  // iterate when we iterate resources.
  LASTRESOURCE,
};

class ResourceConsumption
{
 public:
  ResourceConsumption();
  bool defined;
  /* ResourceType type; DEPRECATED - this info is in its location in the vectors */
  RandomVariable consumption;

  // Type of distribution, and the parameters
  // used by it. Three parameters is enough
  // to describe all distributions currently
  // supported in ns-3.
  std::string distributionType;
  double param1;
  double param2;
  double param3;
};

class RemainingResourceConsumption {
 public:
  bool defined;
  /* ResourceType type; DEPRECATED - this info is in its location in the vectors */
  double amount;
};

class ProcessingStage;

class ProcessingInstance
{
public:
  int factor = 1;
  bool done;
  EventId processingCompleted; // Filled by PEU
  RemainingResourceConsumption remaining[LASTRESOURCE]; // Filled by processing stage
  ProcessingStage *source; // Filled by the processing stage to be itself
  Ptr<Thread> thread;
};

enum ExecutionEventType {
  LOOP,
  EXECUTE,
  PROCESS,
  SCHEDULER,
  SYNCHRONIZATION,
  QUEUE,
  INTERRUPT,
  CONDITION,
  TEMPSYNCH,
  END,
  DEBUG,
  MEASURE,
  LASTTYPE,
};

class Program;

class ExecutionEvent {
 public:
  static std::string typeStrings[LASTTYPE];
  ExecutionEventType type;
  virtual ~ExecutionEvent();

  // We need access to the program via which the
  // HWModel can access a pointer to the PEU.
  // DEPRECATED: we get the PEU via the program
  //    directly in thread.cc
  /* Program *program; */

  // The checkpoint added to this event. Set to
  // the empty string "" if no checkpoint is set.
  std::string checkpoint;
  
  // Contains the original line from the DDF
  // from which this event was created. May
  // be used during parsing and/or
  // execution.
  std::vector<std::string> tokens;

  friend std::ostream& operator<<(std::ostream& out, ExecutionEvent& event);

  // DEBUG:
  std::string line;
  int lineNr;
  bool hasDebug;
  std::string debug;

  ExecutionEvent();
};

class TempCompletion : public ExecutionEvent {
public:
 TempCompletion();
 virtual ~TempCompletion();

 std::vector<std::string> users;
 bool global;

 friend std::ostream& operator<<(std::ostream& out, TempCompletion& event);
};

enum ConditionType {
  QUEUECONDITION,
  SERVICEQUEUECONDITION,
  STATEQUEUECONDITION,
  THREADCONDITION,
  PACKETCHARACTERISTIC,
  STATECONDITION,
  LOOPCONDITION,
};

class Condition : public ExecutionEvent {
 public:
  Condition();
  virtual ~Condition();
  ConditionType condType;
  conditionScope scope;
  std::list<std::pair<uint32_t, Program *> > programs;
  Callback<uint32_t, Ptr<Thread> > getConditionState;
  Callback<void, Ptr<Thread> , uint32_t> setConditionState;
  Callback<uint32_t, Ptr<Queue>, Ptr<Queue> > getConditionQueues;
  Callback<uint32_t, std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *, std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *> getServiceConditionQueues;
  Callback<uint32_t, std::string > getConditionThread;

  // Functions used to insert an entry
  void insertEntry(uint32_t entry, Program *p);

  // Function used to get the distance to the closest entry
  //  uint32_t getDistance(Ptr<Packet> p);

  // Function used to obtain closest entry
  std::pair<uint32_t, Program *> getClosestEntryValue(uint32_t value);
  virtual std::pair<uint32_t, Program *> getClosestEntry(Ptr<Thread> t);


  friend std::ostream& operator<<(std::ostream& out, Condition& event);
};

class QueueCondition : public Condition {
 public:
  QueueCondition();
  virtual ~QueueCondition();

  // In order to save time, we want direct pointer
  // to queues in stead of strings that identify them.
  // The interpreter in thread.cc will use
  // processing->conditionFunctions->QueueCondition(queue)
  // on each queue between firstQueue and lastQueue according
  // to processing->queueOrder.
  Ptr<Queue> firstQueue;
  Ptr<Queue> lastQueue;

  friend std::ostream& operator<<(std::ostream& out, QueueCondition& event);
};

class StateQueueCondition : public Condition {
 public:
  StateQueueCondition();
  virtual ~StateQueueCondition();


  // In order to save time, we want direct pointer
  // to queues in stead of strings that identify them.
  // The interpreter in t|hread.cc will use
  // processing->conditionFunctions->QueueCondition(queue)
  // on each queue between firstQueue and lastQueue according
  // to processing->queueOrder.
  std::string queueName;
};

class ServiceQueueCondition : public Condition {
 public:
  ServiceQueueCondition();
  virtual ~ServiceQueueCondition();


  // In order to save time, we want direct pointer
  // to queues in stead of strings that identify them.
  // The interpreter in thread.cc will use
  // processing->conditionFunctions->QueueCondition(queue)
  // on each queue between firstQueue and lastQueue according
  // to processing->queueOrder.
  std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *firstQueue;
  std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *lastQueue;

  friend std::ostream& operator<<(std::ostream& out, ServiceQueueCondition& event);
};

class ThreadCondition : public Condition {
 public:
  ThreadCondition();
  virtual ~ThreadCondition();

  // NOTE: We cannot do the same as with queues above,
  // because we don't have the threads before we have
  // create the SEMs; the thread mappings allways
  // reside at the end of the device description file,
  // because we need the SEMs in order to dispatch
  // the threads.
  std::string threadId;

  friend std::ostream& operator<<(std::ostream& out, ThreadCondition& event);
};

class PacketCharacteristic : public Condition
{
 public:
  PacketCharacteristic();
  virtual ~PacketCharacteristic();

  friend std::ostream& operator<<(std::ostream& out, PacketCharacteristic& event);
  // TODO: add comparisons
};

class StateCondition : public Condition {
 public:
  StateCondition();
  virtual ~StateCondition();

  // For these, thread.cc will pass NULL as the packet to
  // the getCondition function. It will not use
  // the packet to obtain node or protocol state
  // anyways.
  // TODO: add comparisons
  std::string name;
  conditionOperation operation;
  uint32_t value;
  bool hasSetterFunction;
  bool hasGetterFunction;


  friend std::ostream& operator<<(std::ostream& out, StateCondition& event);
};

class LoopCondition : public Condition {
 public:
  LoopCondition();
  virtual ~LoopCondition();
  // For these, we pass NULL as the packet to
  // the getCondition function. It will not use
  // the packet to obtain node or protocol state
  // anyways.
  //
  // The interpreter in thread.cc will use take
  // the same steps upon "RESTART" which is done
  // on a queue iteration, only it will add the
  // extra (potential) constraint of the maximum
  // number of iterations.

  uint32_t maxIterations;
  bool perQueue; // maxIterations in total, or per queue?
  bool serviceQueues;
  bool stateQueues;
  std::vector<Ptr<Queue> > queuesServed;
  std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *> serviceQueuesServed;
  std::vector<Ptr<StateVariableQueue> > stateQueuesServed;


  // When the conditional for continuing iteration in a loop
  // fails, this may be discovered somewhere in the loop, resulting
  // in a break sentence. I designed the loops as they are now,
  // because _in most cases_, these loop iterations are neglectable
  // in terms of impact on performance. However, I discovered that
  // in the driver, which must consult the NIC via DMA to uncover
  // whether or not the packet is available, such an iteration
  // incurrs non-negletable overhead. Thus, we add here the program
  // that is to be executed when no packet is in the queue(s). This
  // program is added when (1) there is no de-queue statement in the
  // signature and (2) the signature is not empty. In thread.cc, when
  // the queues are empty, this program is executed _once_ if not NULL.
  Program *emptyQueues;

  // The potentially additional condition function
  bool hasAdditionalCondition;
  Callback<uint32_t, Ptr<Thread> > additionalCondition;

  friend std::ostream& operator<<(std::ostream& out, LoopCondition& event);
  // TODO: add comparisons
};

class InterruptExecutionEvent : public ExecutionEvent {
 public:
  InterruptExecutionEvent(int IRQNr);
  virtual ~InterruptExecutionEvent();
  std::string service;
  int number;

  friend std::ostream& operator<<(std::ostream& out, InterruptExecutionEvent& event);
};


class ProcessingStage : public ExecutionEvent {
 public:
  ProcessingStage();
  virtual ~ProcessingStage();

  ResourceConsumption resourcesUsed[LASTRESOURCE];
  ProcessingInstance Instantiate();
  uint32_t samples; // The number of samples observed
  Ptr<SEM> interrupt;
  int factor = 1;  // Factor to multiply processing time with
  Ptr<Queue> pktqueue;  // Queue to dequeue from and set factor to multiply with that packet.

  friend std::ostream& operator<<(std::ostream& out, ProcessingStage& event);
};

class SchedulerExecutionEvent : public ExecutionEvent {
public:
  SchedulerExecutionEvent(int type, std::vector<uint32_t> arguments, std::string);
  virtual ~SchedulerExecutionEvent();

  int schedType;
  std::vector<uint32_t> args;
  std::string threadName;

  friend std::ostream& operator<<(std::ostream& out, SchedulerExecutionEvent& event);
};

class SynchronizationExecutionEvent : public ExecutionEvent {
public:
  SynchronizationExecutionEvent();
  virtual ~SynchronizationExecutionEvent();

  int synchType;
  std::string id;
  std::vector<uint32_t> args;
  bool temp;
  bool global;

  friend std::ostream& operator<<(std::ostream& out, SynchronizationExecutionEvent& event);
};

class QueueExecutionEvent : public ExecutionEvent {
public:
	QueueExecutionEvent();
	virtual ~QueueExecutionEvent();

	bool enqueue;
	bool serviceQueue;
	bool stateQueue;
	bool local;
	std::string threadToWake;
	Ptr<Queue> queue;
	std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *servQueue;
	std::string queueName;

	// If we have an enqeueue with a service, we must
	// store it here.
	Ptr<SEM> semToEnqueue;

	// If we have a state queue
	uint32_t valueToEnqueue;
	  friend std::ostream& operator<<(std::ostream& out, QueueExecutionEvent& event);
};

class SEM;

class ExecuteExecutionEvent : public ExecutionEvent {
public:
  ExecuteExecutionEvent();
  virtual ~ExecuteExecutionEvent();

  LoopCondition *lc;
  std::string service;
  Ptr<SEM> sem;

  friend std::ostream& operator<<(std::ostream& out, ExecuteExecutionEvent& event);
};

class DebugExecutionEvent : public ExecutionEvent {
 public:
  DebugExecutionEvent();
  virtual ~DebugExecutionEvent();

  std::string tag;

  friend std::ostream& operator<<(std::ostream& out, DebugExecutionEvent& event);
};

class MeasureExecutionEvent : public ExecutionEvent {
 public:
  MeasureExecutionEvent();
  virtual ~MeasureExecutionEvent();

  // std::string tag;

  friend std::ostream& operator<<(std::ostream& out, MeasureExecutionEvent& event);
};


class Program
{
 public:
	Program();
  std::vector<ExecutionEvent *> events;
  Ptr<SEM> sem;

  // Need to know if it contains a dequeue OR an internal loop
  // handling the queues regarded. This affects the behavior
  // or REITERATE and LOOP: if we have an internal loop, this
  // loop is only there to "continue executing the body as
  // long as there is content in one or more of the queues",
  // i.e., we do not update any current queue because upon hitting
  // the last queue in the queue order, the loop would exit regardless
  // of whether a queue
  // before the current queue in the queue order was inserted
  // into in the mean time. The semantics of this loop, explained
  // above, would there by be violated. IF we have a dequeue, on the
  // other hand, we MUST update the current queue. Note that this
  // would prevent the loop from "going back" to an earlier queue
  // when insertions into any of these were performed in the mean
  // time, but this is why we have nested queues in the first place,
  // and thus should thus not occur assuming correct instrumentation.
  bool hasInternalLoop;
  bool hasDequeue;
};

} // Namespace ns3

#endif /* PROGRAM_H */
