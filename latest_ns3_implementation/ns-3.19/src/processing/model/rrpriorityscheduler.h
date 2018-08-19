#ifndef RRSCHEDULER_H
#define RRSCHEDULER_H

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/local-state-variable-queue.h"
#include "ns3/local-state-variable.h"
//#include "ns3/packet.h"

//#include "program.h"
//#include "thread.h"
//#include "peu.h"

#include <ns3/sync.h>

#include <vector>
#include <map>
#include <list>
#include <set>

namespace ns3 {

class RoundRobinPriorityScheduler : public TaskScheduler
{
public:
  static TypeId GetTypeId (void);

  RoundRobinPriorityScheduler();
  virtual ~RoundRobinPriorityScheduler();

  void Schedule();

  // To keep track of mapping between PID and thread name.
  // The latter is used with scheduler requests.
  // std::map<std::string, int> threadPids;

  // To start and handle the scheduling events
  // virtual void Initialize(Ptr<PEU> cpuPEU);
  // void HandleSchedulerEvent();

  // Functions to install, remove and switch
  // between threads
  // virtual void Terminate(unsigned int pid); // Terminates the current running

  /*
  void PreEmpt(int new_pid);

  // Functions to issue requests to the scheduler
  bool Request(int type, std::vector<uint32_t> arguments);
  bool SynchRequest(int type, std::string id, std::vector<uint32_t> arguments);
  bool TempSynchRequest(int type, void* var, std::vector<uint32_t> arguments);
  void AllocateSynch(int type, std::string id, std::vector<uint32_t> arguments);
  void *AllocateTempSynch(int type, std::vector<uint32_t> arguments);
  void DeallocateTempSynch(void* var);
  int CurrentRunning(void);

  // Used to get the synch type during parsing
  uint32_t GetSynchReqType(std::string name);
  */

  // Ptr<Thread> m_currentRunning;
  
  std::map<int, std::list<int> > m_runqueues;
  std::set<int> m_blocked;
  std::vector<int> m_currentRunning;

  std::map<int, ns3::Time> m_remainingQuantum;
  ns3::Time m_lastSchedulingTime;

  // unsigned int m_currentRunningPid;
  // unsigned int m_nextpid;

protected:
  void UpdateQuantums();
  void RescheduleCPU(int);
  // std::map<int, Ptr<Thread> > m_threads;

  void PreEmpt(int cpu, int new_pid);

  // The functions below must be implemented by a subclass
  // for a specific SchedSim
  virtual void DoInitialize();
  virtual void DoHandleSchedulerEvent();
  virtual int DoFork(int priority);
  virtual void DoTerminate(void);
  virtual std::vector<int> DoCurrentRunning(void);
  virtual void DoAllocateSynch(int type, std::string id, std::vector<uint32_t> arguments);
  virtual void* DoAllocateTempSynch(int type, std::vector<uint32_t> arguments);
  virtual void DoDeallocateTempSynch(void* var);
  virtual int DoRequest(int cpu, int type, std::vector<uint32_t> arguments);
  virtual int DoSynchRequest(int cpu, int type, std::string id, std::vector<uint32_t> arguments);
  virtual int DoTempSynchRequest(int cpu, int type, void *var, std::vector<uint32_t> arguments);
  virtual uint32_t DoGetSynchReqType(std::string name);

  void MigrateThread(int pid, Ptr<Thread> thread);

  Ptr<PEU> peu;
private:

  std::map<std::string, Ptr<Semaphore> > m_semaphores;
};

}

#endif
