#ifndef PEU_H
#define PEU_H

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "sharedresource.h"
#include "ns3/packet.h"

#include "thread.h"
//#include "taskscheduler.h"
//#include "hwmodel.h"
//#include "interrupt-controller.h"

#include <vector>
#include <stack>
#include <map>

namespace ns3 {

class TaskScheduler;
class ProcessingInstance;
class InterruptRequest;
class HWModel;
class SEM;

class PEU : public Object
{
public:
  // All PEUs have a taskscheduler, however, usually
  // only the CPU is using it.
  static TypeId GetTypeId (void);

  PEU();
  PEU(std::string name, long freq);
  virtual ~PEU ();

  // Calculates cycles and/or milliseconds spent on the PEU,
  // and updates the utilization and duration of other
  // processing instances. It assumes uniform distirbution
  // of consumption of units in time from the shared resources.
  void Consume(ProcessingInstance *pi);

  // Needed to access other PEUs as well as shared
  // resources. Currently we only model the memory
  // bus as a shared resource, i.e., we do not
  // currently support caches. Nor do we support
  // complex bus-hierarchies; we simply model the
  // bus as a black-box (the method added to
  // paper about methodology).
  Ptr<HWModel> hwModel;
  std::string m_name;

  virtual bool IsCPU() { return false; }

  // Vector to hold ongoing threads
  std::vector<Ptr<Thread> > currentlyRunningThreads;

  // The SEM describing the program run by this PEU
  // Note that PEUs may en/de-queue, have a current
  // packet in each thread used to parameterize this
  // SEM, the SEM processes and finally usually
  // issues an interrupt.
  Ptr<SEM> sem;

  uint32_t m_tracingOverhead;

  long m_freq; // Important!: Defined in KHz


  // DEPRECATED: which interrupt number to use
  //   is specified in the program by a
  //   statement "EXECUTE interrupt::21".
  /* int m_interruptNr; */

  Ptr<RoundRobinScheduler> taskScheduler;
};

// The CPU is a special PEU:
// (1) Is time shared by LEUs, that is managed
//     by a TaskScheduler.
// (2) Is the object receiving interrupts
// (3) Does not itself generate interrupts (future work)
class CPU : public PEU
{
 public:
  // Returns false if the interrupt was
  // not handeled due to interrupts turned
  // off.

  CPU();
  static TypeId GetTypeId (void);
  Ptr<Thread> interruptThread;
  std::string hirqHandler;
  std::string hirqQueue;

  bool IsCPU() { return true; }

  bool Interrupt(InterruptRequest ir);
  void EnableInterrupts();
  void DisableInterrupt();
  bool inInterrupt;

  bool interruptsEnabled;

  int GetId();

  // We re-use the Thread class to model interrupts.
  // We do not model nested nor shared
  // interrupts as for now. When we ass support
  // for nested interrupt execution, we must change
  // this data structure to a stack.
};

} // namespace ns3

#endif /* PEU_H */
