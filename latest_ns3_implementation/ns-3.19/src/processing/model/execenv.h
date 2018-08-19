#ifndef PROCESSING_H
#define PROCESSING_H

#include <vector>
#include <map>
#include <queue>

#include "ns3/object.h"
#include "ns3/empty.h"
#include "ns3/event-impl.h"
#include "ns3/random-variable.h"
#include "ns3/callback.h"
#include "ns3/simulator.h"
#include "ns3/type-traits.h"
#include "ns3/net-device.h"
#include "ns3/make-event.h"
#include "ns3/queue.h"
#include "ns3/packet.h"

#include "peu.h"
#include "program.h"
#include "condition.h"

// Constants used during parsing
#define LOCAL_CONDITION 0
#define GLOBAL_CONDITION 1
#define QUEUEEMPTY 0
#define QUEUENOTEMPTY 1
#define THREADREADY 0
#define THREADNOTREADY 1

namespace ns3 {

class HWModel;
class SEM;
class ConditionFunctions;

#define PROCESSING_DEBUG 0

class ExecEnv : public Object
{
public:
  ExecEnv();
  static TypeId GetTypeId (void);

  /* Interface with FCMs */
  template<class MEM, class OBJ> bool Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object);
  template<class T1, class MEM, class OBJ> bool Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1);
  template<class T1,class T2, class MEM, class OBJ> bool Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2);
  template<class T1,class T2, class T3, class MEM, class OBJ> bool Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3);
  template<class T1,class T2, class T3, class T4, class MEM, class OBJ> bool Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3, T4 arg4);
  template<class T1,class T2, class T3, class T4, class T5, class MEM, class OBJ> bool Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5);
  template<class T1,class T2, class T3, class T4, class T5, class T6, class MEM, class OBJ> bool Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6);

  // The average number of CPU-cycles spent per trace-call
  // This is deducted during ProcessingStage::Instantiate()
  // for the resource "cycles" on the CPU.
  uint32_t m_traceOverhead;

  // Holds functions used to resolve conditions
  Ptr<ConditionFunctions> conditionFunctions;

  Ptr<RoundRobinScheduler> cpuScheduler;

  // Holds global state variables
  std::map<std::string, uint32_t > globalStateVariables;

  // Holds all intra-node queues used during
  // execution.
  std::map<std::string, Ptr<Queue> > queues;
  std::map<Ptr<Queue>, std::string> queueNames;
  std::vector<Ptr<Queue> > queueOrder;

  // Service queues hold the current packet to set, as well as the
  // service to call.
  std::map<std::string, std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *> serviceQueues;
  std::map<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *, std::string> serviceQueueNames;
  std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *> serviceQueueOrder;

  // State queues only hold a set of values.
  //
  // DEPRECATED: Since we only support local state queues,
  // we only store their order, as this is needed with the current design of loops.
  //
  // 210814: We DO support global state queues, in face we ONLY support that now. We had
  // to move to this to allow collaborating threads to use the same state queue, and
  // we currently have no way to pass local variables like state queues between threads.
  // Thus, we had to allow two collaborating threads to share a state queue via global
  // scope, i.e., via ExecEnv (see above). Due to time constraints, we still keep track
  // of state queue order only via straings.
  std::vector<std::string> stateQueueOrder;
  std::map<std::string, Ptr<StateVariableQueue> > stateQueues;
  std::map<Ptr<StateVariableQueue>, std::string> stateQueueNames;

  /* Contains all services: m_serviceMap according to
     real world function names, and serviceTriggermap
     according to trigger names. The last two maps,
     are maps between the two
     types of keys. Note that currently, the SEMs pointed
     to in serviceTriggerMap must be a subset of
     m_serviceMap. */
  std::map<std::string, Ptr<SEM> > m_serviceMap;
  std::map<std::string, Ptr<SEM> > serviceTriggerMap;
  std::map<std::string, std::string> triggerToRWFunc;
  std::map<std::string, std::string> RWFuncToTrigger;

  // A map between real world service names and trigger names

  // We want a map between service triggers and the SEM for cases
  // where we want to call the "next" in the packet.

  /* Called to parameterize all services according to a device
     description file */
  void Initialize(std::string device);

  // The hardware model containing:
  // - Interrupt controller
  // - memory bus
  // - all PEUs present in the system
  //     - Which in turn contain threads and taskschedulers
  Ptr<HWModel> hwModel;

 //private:
  // To handle different sections of the device file
  void HandleQueue(std::vector<std::string> tokens);
  void HandleSynch(std::vector<std::string> tokens);
  void HandleThreads(std::vector<std::string> tokens);
  void HandleHardware(std::vector<std::string> tokens);
  void HandleConditions(std::vector<std::string> tokens);
  void HandleTriggers(std::vector<std::string> tokens);
  void HandleSignature(std::vector<std::string> tokens);

  // Helper functions for Parse
  double stringToDouble(const std::string& s);
  uint32_t stringToUint32(const std::string& s);
  std::string deTokenize(std::vector<std::string> tokens);
  void PrintProgram(Program *curPgm);
  void PrintSEM(Program *curPgm, int numIndent);
  bool queuesIn(std::string first, std::string last, LoopCondition *lc);
  void fillQueues(std::string first, std::string last, LoopCondition *lc);
  // To parse device files
  void Parse(std::string device);

  // Executed upon STOP/RESTART event to insert
  // newly created program into existing SEM
  void addPgm(Program *curPgm, Program* existPgm);
  void delPgm(Program *toDelete);
  ProcessingStage addProcessingStages(ProcessingStage a, ProcessingStage b);

  // Used to store condition information temporarily
  // while parsing signatures
  std::map<std::string, struct condition> locationConditions;
  std::map<std::string, std::vector<struct condition> > dequeueConditions;
  std::map<std::string, std::vector<struct condition> > enqueueConditions;
  std::map<std::string, struct condition > loopConditions;

  // Variables to hold data regarding triggers. Three
  // types of triggers: services, de-queue operations
  // and locations.
  std::map<std::string, std::string> serviceTriggers;
  std::map<std::string, std::string> dequeueTriggers;
  std::map<std::string, std::string> locationTriggers;

  std::vector<struct tempVar> tempVars;
};

  /* If we were executed by the ExecEnv, we first tell the next
     request to the ExecEnv that we were not executed by it
     (by setting it to false), and then return false to tell
     the calling FPM that its execution delay has been
     introduced allready. If we were not executed by the ExecEnv, we insert the
     provided function and arguments into the packet after
     first creating an event of it. We also insert the
     target ID into the packet for ideintification
     by checkpoint encountered by the ExecEnv some time in
     the future, upon which the above mentioned event is invoked.
     We finally return true indicating to the calling FPM that
     execution delay is yet to be introduced. We also
     set the executedByExecEnv to true indicating upon the next
     call to Proceed that execution delay has already been
     introduced. */

template<class MEM, class OBJ> bool ExecEnv::Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object) {
  if(packet->m_executionInfo.executedByExecEnv) { packet->m_executionInfo.executedByExecEnv = false; return false;}
  else { packet->m_executionInfo.target = target; packet->m_executionInfo.targetFPM = MakeEvent(func, object); packet->m_executionInfo.executedByExecEnv = true; return true;}
}

template<class T1, class MEM, class OBJ> bool ExecEnv::Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1) {
  if(packet->m_executionInfo.executedByExecEnv) { packet->m_executionInfo.executedByExecEnv = false; return false; }
  else { packet->m_executionInfo.target = target; packet->m_executionInfo.targetFPM = MakeEvent(func, object, arg1); packet->m_executionInfo.executedByExecEnv = true; return true; }
}

template<class T1,class T2, class MEM, class OBJ> bool ExecEnv::Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2) {
  if(packet->m_executionInfo.executedByExecEnv) { packet->m_executionInfo.executedByExecEnv = false; return false;}
  else { packet->m_executionInfo.target = target; packet->m_executionInfo.targetFPM = MakeEvent(func, object, arg1, arg2); packet->m_executionInfo.executedByExecEnv = true; return true;}
}

 template<class T1,class T2, class T3, class MEM, class OBJ> bool ExecEnv::Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3) {
  if(packet->m_executionInfo.executedByExecEnv) { packet->m_executionInfo.executedByExecEnv = false; return false;}
  else { packet->m_executionInfo.target = target; packet->m_executionInfo.targetFPM = MakeEvent(func, object, arg1, arg2, arg3); packet->m_executionInfo.executedByExecEnv = true; return true;}
}

 template<class T1,class T2, class T3, class T4, class MEM, class OBJ> bool ExecEnv::Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3, T4 arg4) {
  if(packet->m_executionInfo.executedByExecEnv) { packet->m_executionInfo.executedByExecEnv = false; return false;}
  else { packet->m_executionInfo.target = target; packet->m_executionInfo.targetFPM = MakeEvent(func, object, arg1, arg2, arg3, arg4); packet->m_executionInfo.executedByExecEnv = true; return true;}
}

 template<class T1,class T2, class T3, class T4, class T5, class MEM, class OBJ> bool ExecEnv::Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5) {
  if(packet->m_executionInfo.executedByExecEnv) { packet->m_executionInfo.executedByExecEnv = false; return false;}
  else { packet->m_executionInfo.target = target; packet->m_executionInfo.targetFPM = MakeEvent(func, object, arg1, arg2, arg3, arg4, arg5); packet->m_executionInfo.executedByExecEnv = true; return true;}
}

 template<class T1,class T2, class T3, class T4, class T5, class T6, class MEM, class OBJ> bool ExecEnv::Proceed(Ptr<Packet> packet, std::string target, MEM func, OBJ object, T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6) {
  if(packet->m_executionInfo.executedByExecEnv) { packet->m_executionInfo.executedByExecEnv = false; return false;}
  else { packet->m_executionInfo.target = target; packet->m_executionInfo.targetFPM = MakeEvent(func, object, arg1, arg2, arg3, arg4, arg5, arg6); packet->m_executionInfo.executedByExecEnv = true; return true;}
}

} // namespace ns3

#endif
