#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

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

#include <vector>
#include <map>

#ifndef NUM_CPU
#define NUM_CPU 128
#endif

namespace ns3 {

class Thread;
class Program;
class PEU;
class Packet;

class TaskScheduler : public Object
{
public:
  static TypeId GetTypeId (void);

  TaskScheduler ();
  virtual ~TaskScheduler ();

  // To keep track of mapping between PID and thread name.
  // The latter is used with scheduler requests.
  std::map<std::string, int> threadPids;

  // To start and handle the scheduling events
  virtual void Initialize(Ptr<PEU> cpuPEU);
  void HandleSchedulerEvent();

  // Functions to install, remove and switch
  // between threads
  virtual Ptr<Thread> Fork(std::string,
			Program *,
			int,
			Ptr<Packet>,
			std::map<std::string, Ptr<StateVariable> >,
			std::map<std::string, Ptr<StateVariableQueue> >,
			bool);
  virtual void Terminate(Ptr<PEU> peu, unsigned int pid); // Terminates the current running
  void PreEmpt(int cpu, int new_pid);

  // Functions to issue requests to the scheduler
  bool Request(int cpu, int type, std::vector<uint32_t> arguments);
  bool SynchRequest(int cpu, int type, std::string id, std::vector<uint32_t> arguments);
  bool TempSynchRequest(int cpu, int type, void* var, std::vector<uint32_t> arguments);
  void AllocateSynch(int type, std::string id, std::vector<uint32_t> arguments);
  void *AllocateTempSynch(int type, std::vector<uint32_t> arguments);
  void DeallocateTempSynch(void* var);
  std::vector<int> CurrentRunning(void);

  // Used to get the synch type during parsing
  uint32_t GetSynchReqType(std::string name);

  // Ptr<Thread> m_currentRunning;
  // std::vector<int> m_currentRunning;
  int m_currentRunning[NUM_CPU];

  Ptr<Thread> GetCurrentRunningThread(int cpu);

  bool CheckAllCPUsForPreemptions(int cpu, std::vector<int> previouslyRunning);

protected:
  std::map<int, Ptr<Thread> > m_threads;

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

  Ptr<PEU> peu;

private:
  void SetupIdleThreads();
};

class InterruptScheduler : public TaskScheduler
{
 protected:
  virtual void DoTerminate();
};

// This maximum number of threads are created upon
// initialization, and their PIDs have increasing
// numbers. m_cutrrentRunning is not used, and
// m_threads contain all running threads. A bitmap
// m_runningBitmap is used to tell which threads
// are running, and which not. DoTerminate() and
// Activate() manipulate this bitmap.
class ParallelThreadsScheduler : public TaskScheduler
{
 public:
  static TypeId GetTypeId(void);

  ParallelThreadsScheduler();

  virtual void Initialize(Ptr<PEU> noncpuPEU);
  virtual void Terminate(unsigned int pid);
  virtual Ptr<Thread> Fork(std::string,
			Program *,
			int,
			Ptr<Packet>,
			std::map<std::string, Ptr<StateVariable> >,
			std::map<std::string, Ptr<StateVariableQueue> >,
			bool);

 private:
  // The maximum number of parallel threads running.
  unsigned int m_maxThreads;
  std::list<unsigned int> m_freeList;
};

} // namespace ns3

#endif /* TASKSCHEDULER_H */
