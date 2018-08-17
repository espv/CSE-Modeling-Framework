#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/program.h"
#include "ns3/uinteger.h"

#include "taskscheduler.h"
#include "thread.h"
#include "peu.h"
#include "hwmodel.h"
#include "execenv.h"
#include "interrupt-controller.h"
#include "sem.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TaskScheduler");
NS_OBJECT_ENSURE_REGISTERED (TaskScheduler);

TypeId 
TaskScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::processing::TaskScheduler")
    .SetParent<Object> ()
    .AddConstructor<TaskScheduler> ();
  return tid;
}

TaskScheduler::~TaskScheduler () {
}

TaskScheduler::TaskScheduler() 
{
    for (int i = 0; i < NUM_CPU; ++i) {
        m_currentRunning[i] = -1;
    }
}

Ptr<Thread>
TaskScheduler::GetCurrentRunningThread(int cpu) {
    return m_threads[m_currentRunning[cpu]];
}

/*
 * CheckForPreemptions checks the vector of current running pids, and issues any 
 * neccessary preemptions. Returns false if the specified cpu has a new CR,
 * ie. the currently running thread should stop executing.
 * Returns true if the thread on the specified cpu should continue executing.
 * m_currentRunning is updated in PreEmpt().
 */
bool
TaskScheduler::CheckAllCPUsForPreemptions(int cpu, std::vector<int> previouslyRunning) {
    bool ret = true;
    std::vector<int> currentRunning = DoCurrentRunning();
    for(unsigned int i = 0; i < peu->hwModel->cpus.size(); i++) {
        if (currentRunning[i] != previouslyRunning[i]) {
            PreEmpt(i, currentRunning[i]);
            if (i == (unsigned int)cpu) ret = false;
        }
    }

    return ret;
}

void
TaskScheduler::Terminate(Ptr<PEU> peu, unsigned int pid)
{
    NS_LOG_INFO("Terminating " << pid);
#if 1
    Ptr<CPU> cpu = peu->GetObject<CPU>();
    if (cpu->inInterrupt) {
        cpu->inInterrupt = false;
        cpu->hwModel->m_interruptController->Proceed(cpu->GetId());
    } else {
        NS_ASSERT(0);
    }
#else
    const int cpu = peu->GetObject<CPU>()->GetId();
	if(peu->hwModel->cpus[cpu]->inInterrupt) {
		peu->hwModel->cpus[cpu]->inInterrupt = false;
		this->peu->hwModel->m_interruptController->Proceed(peu->GetObject<CPU>()->GetId());
	} else {
        // OYSTEDAL: cr used to determine which thread to terminate.
        // Should come from the parameter instead.
		m_threads[m_currentRunning->GetPid()] = NULL;
		DoTerminate();
	}
#endif
}

// Must be implemented by a SimSched-specific sub-class
bool
TaskScheduler::Request(int cpu, int type, std::vector<uint32_t> arguments)
{
#if 1
    std::vector<int> previouslyRunning = std::vector<int>(m_currentRunning, m_currentRunning + NUM_CPU);
    DoRequest(cpu, type, arguments);

    return CheckAllCPUsForPreemptions(cpu, previouslyRunning);
#else
	  int newCR = DoRequest(type, arguments);

	  //
	  // When we have a new current running, we return
	  // true. The calling thread will then stop executing.
	  // Note that we cannot simply dispatch the new thread
	  // here, i.e., from within this event, as that would
	  // result in the new thread executing within the
	  // synchronization event of the former thread. This
	  // could result in unpredictable behavior. In stead,
	  // we use the NetSim scheduler to schedule the dispatch
	  // of the new thread at this time instance, resulting
	  // in the new thread executing in a new event, and
	  // allowing the previous thread to return completely
	  // from its execution, i.e., its Dispatch(). Note also
	  // that no further operations are necessary here, as
	  // the task switch consists of (1) the previous thread
	  // stopping its execution since we return false, and
	  // (2) the new thread starting its execution immediately
	  // in virtual time due to our scheduling its Dispatch()
	  // at this point in time. In addition, we (3) update the
	  // current running pid.
	  //

      // OYSTEDAL: CR used to determine if a context switch occurred.
      // For multicore, this is the case when the vector of current running 
      // threads have changed. If it has, we need to preemt any cores that are
      // now executing a new thread.
	  if(m_currentRunning->GetPid() != newCR) {
		PreEmpt(newCR);
	    return false;
	  }
	  else
	    return true;
#endif
}

// Must be implemented by a SimSched-specific sub-class
std::vector<int>
TaskScheduler::CurrentRunning(void)
{
  return std::vector<int>();
}

void
TaskScheduler::HandleSchedulerEvent()
{
  // Obtain iterator
  //  Simulator::Schedule(m_timeslice, &TaskScheduler::HandleSchedulerEvent, this);
}

// Creates and initializes data structures for this node as well as 
// creating the idle thread.
void
TaskScheduler::Initialize(Ptr<PEU> cpuPEU)
{
  // Set the CPU
  peu = cpuPEU;

  /*

  // Create resource consumption for idle thread
  ResourceConsumption idleResources;
  idleResources.consumption = NormalVariable(10000000000000, 0);
  idleResources.defined = true;

  // Create processing stage
  ProcessingStage *idleProcessing = new ProcessingStage();

  idleProcessing->resourcesUsed[NANOSECONDS] = idleResources;

  // Create idle program and store in program:
  // - Infinite loop start
  // - process for one million seconds
  // - infinite loop end
  Program *idleProgram = new Program();
  idleProgram->events.push_back(idleProcessing);
  ExecutionEvent *end = new ExecutionEvent();
  end->type = END;
  idleProgram->events.push_back(end);

  // Create and register a SEM, in which we only store the CPU as
  // the PEU. This is because the thread calls Consume()
  // of the peu on which the program runs. We do not need
  // to create any measurements, though, as the thread
  // should (!) never call execute, i.e., it is never
  // allowed to execute the idle program from another
  // thread.
  Ptr<SEM> idleSEM= Create<SEM> ();
  idleSEM->name = "idlethread";
  idleSEM->peu = peu;
  peu->hwModel->node->GetObject<ExecEnv> ()->m_serviceMap["idle"] = idleSEM;
//  std::cout << "idleProgram->sem before setting it: " << idleProgram->sem << std::endl;
  idleProgram->sem = idleSEM;
//  std::cout << "idleProgram->sem after setting it: " << idleProgram->sem << std::endl;
    */

  Simulator::ScheduleNow(&TaskScheduler::SetupIdleThreads, this);

  // Call SchedSim-specific DoStart
  DoInitialize();

  // Create a thread of execution - it must be
  // set up to call the idleProgram in an endless
  // loop
  /*
  ProgramLocation toe;
  toe.program = idleProgram;
  toe.currentEvent = -1; // Will be incremented by Dispatch()
  toe.lc = new LoopCondition();
  toe.lc->emptyQueues = idleProgram;
  toe.lc->maxIterations = 0;
  toe.lc->perQueue = false;
  */

  // The current running in the scheduler simulator must
  // be the idle thread, since no other threads have been
  // created yet.
  
  // OYSTEDAL: We assume one idle thread per cpu, hence there is a set of pids
  // corresponding to the idle thread of each cpu.
#if 0
  int idleThreadPid = DoCurrentRunning();
  Ptr<Thread> idleThread = CreateObject<Thread> ();
  idleThread->SetPid(idleThreadPid);
  idleThread->m_programStack.push(toe);

//  std::cout << "Program stack during init: " << idleThread->m_programStack.top().program << std::endl;

  // Add this to the threads, and dispatch
  m_threads[idleThreadPid] = idleThread;
  m_currentRunning = idleThread; // OYSTEDAL: Set the CR of each cpu to the idle thread
  idleThread->Dispatch(); // OYSTEDAL: Dispatch each idle thread

  NS_LOG_INFO("Task scheduler initialized. PID of idle thread is " << idleThreadPid);
#else
  /*
  std::vector<int> idleThreadPids = DoCurrentRunning();
  for (int i = 0; i < NUM_CPU; i++) {
      Ptr<Thread> idleThread = CreateObject<Thread>();
      idleThread->SetPid(idleThreadPids[i]);
      idleThread->m_programStack.push(toe);

      m_currentRunning[i] = idleThreadPids[i];

      m_threads[idleThreadPids[i]] = idleThread;

      idleThread->peu = this->peu->hwModel->cpus[i];
      idleThread->Dispatch();
  }

  // NS_ASSERT(m_currentRunning.size() == NUM_CPU);
  */
#endif

}

void
TaskScheduler::SetupIdleThreads()
{
  // Create resource consumption for idle thread
  ResourceConsumption idleResources;
  idleResources.consumption = NormalVariable(10000000000000, 0);
  idleResources.defined = true;

  // Create processing stage
  ProcessingStage *idleProcessing = new ProcessingStage();

  idleProcessing->resourcesUsed[NANOSECONDS] = idleResources;

  // Create idle program and store in program:
  // - Infinite loop start
  // - process for one million seconds
  // - infinite loop end
  Program *idleProgram = new Program();
  idleProgram->events.push_back(idleProcessing);
  ExecutionEvent *end = new ExecutionEvent();
  end->type = END;
  idleProgram->events.push_back(end);

  // Create and register a SEM, in which we only store the CPU as
  // the PEU. This is because the thread calls Consume()
  // of the peu on which the program runs. We do not need
  // to create any measurements, though, as the thread
  // should (!) never call execute, i.e., it is never
  // allowed to execute the idle program from another
  // thread.
  Ptr<SEM> idleSEM= Create<SEM> ();
  idleSEM->name = "idlethread";
  idleSEM->peu = peu;
  peu->hwModel->node->GetObject<ExecEnv> ()->m_serviceMap["idle"] = idleSEM;
//  std::cout << "idleProgram->sem before setting it: " << idleProgram->sem << std::endl;
  idleProgram->sem = idleSEM;
//  std::cout << "idleProgram->sem after setting it: " << idleProgram->sem << std::endl;

  Ptr<ProgramLocation> toe = Create<ProgramLocation>();
  toe->program = idleProgram;
  toe->currentEvent = -1; // Will be incremented by Dispatch()
  toe->lc = new LoopCondition();
  toe->lc->emptyQueues = idleProgram;
  toe->lc->maxIterations = 0;
  toe->lc->perQueue = false;

    std::vector<int> idleThreadPids = DoCurrentRunning();
    for (unsigned int i = 0; i < this->peu->hwModel->cpus.size(); i++) {
        Ptr<Thread> idleThread = CreateObject<Thread>();
        idleThread->SetPid(idleThreadPids[i]);
        idleThread->m_programStack.push(toe);

        m_currentRunning[i] = idleThreadPids[i];

        m_threads[idleThreadPids[i]] = idleThread;

        idleThread->peu = this->peu->hwModel->cpus[i];
        idleThread->Dispatch();
    }
}

void
TaskScheduler::AllocateSynch(int type, std::string id, std::vector<uint32_t> arguments)
{
  return DoAllocateSynch(type, id, arguments);
}


bool
TaskScheduler::SynchRequest(int cpu, int type, std::string id,  std::vector<uint32_t> arguments)
{
#if 1
  std::vector<int> previouslyRunning = DoCurrentRunning();

  /*int newCR = */DoSynchRequest(cpu, type, id, arguments);

  return CheckAllCPUsForPreemptions(cpu, previouslyRunning);
#else

  //
  // When we have a new current running, we return
  // true. The calling thread will then stop executing.
  // Note that we cannot simply dispatch the new thread
  // here, i.e., from within this event, as that would
  // result in the new thread executing within the
  // synchronization event of the former thread. This
  // could result in unpredictable behavior. In stead,
  // we use the NetSim scheduler to schedule the dispatch
  // of the new thread at this time instance, resulting
  // in the new thread executing in a new event, and
  // allowing the previous thread to return completely
  // from its execution, i.e., its Dispatch(). Note also
  // that no further operations are necessary here, as
  // the task switch consists of (1) the previous thread
  // stopping its execution since we return false, and
  // (2) the new thread starting its execution immediately
  // in virtual time due to our scheduling its Dispatch()
  // at this point in time. In addition, we (3) update the
  // current running pid.
  //
  
  // OYSTEDAL: As previously, determine if the vector of currently running
  // threads have changed, and if so, one should preempt any of the threads that
  // have been switched out. TODO: Make a function for this.
  if(m_currentRunning->GetPid() != newCR) {
	PreEmpt(newCR);
    return false;
  }
  else
    return true;
#endif
}

uint32_t
TaskScheduler::GetSynchReqType(std::string name)
{
  return DoGetSynchReqType(name);
}

uint32_t
TaskScheduler::DoGetSynchReqType(std::string name)
{
  NS_LOG_ERROR("DoGetSynchRequest not implemented in SchedSim");
  return -1;  
}

int
TaskScheduler::DoSynchRequest(int cpu, int type, std::string id,  std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoSynchRequest not implemented in SchedSim");
  return -1;
}

int
TaskScheduler::DoTempSynchRequest(int cpu, int type, void *var, std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoTempSynchRequest not implemented in SchedSim");
  return -1;
}

bool TaskScheduler::TempSynchRequest(int cpu, int type, void* var, std::vector<uint32_t> arguments){
    std::vector<int> previouslyRunning = DoCurrentRunning();

    /*int newCR = */DoTempSynchRequest(cpu, type, var, arguments);

    return CheckAllCPUsForPreemptions(cpu, previouslyRunning);

#if 0
//  std::cout << "Current running in TempSynchRequest: " << m_currentRunning->GetPid() << std::endl;
	int newCR = DoTempSynchRequest(type, var, arguments);

    // OYSTEDAL: Again, CR is used to detect if a preemption has occurred.
    // Temp synchs are commonly used during interrupts, and we don't want to
    // stop executing the interrupt
	if(m_currentRunning->GetPid() != newCR) {
		PreEmpt(newCR);
		if(this->peu->hwModel->cpus[0]->inInterrupt)
			return true;
		else
			return false;
	}
	else
#endif
		return true;
}

void TaskScheduler::DeallocateTempSynch(void* var) {
	DoDeallocateTempSynch(var);
}

void
TaskScheduler::PreEmpt(int cpu, int new_pid)
{
#if 1
    Ptr<Thread> cr = GetCurrentRunningThread(cpu);

    if (!this->peu->hwModel->cpus[cpu]->inInterrupt)
        if (!cr->m_currentProcessing.done) // TODO OYSTEDAL: We should possibly have a CR here
            cr->PreEmpt();

    int oldPid = m_currentRunning[cpu];
    m_currentRunning[cpu] = new_pid;

    NS_LOG_INFO("CPU" << cpu << ": Context switch " << oldPid << " -> " << new_pid);

    Ptr<Thread> new_cr = GetCurrentRunningThread(cpu);

    new_cr->peu = this->peu->hwModel->cpus[cpu];

    if (!this->peu->hwModel->cpus[cpu]->inInterrupt)
                  Simulator::ScheduleNow(&Thread::Dispatch, new_cr);

#else
    // OYSTEDAL: Current running used to detect the need for preemption in the
    // currently running thread. 
    
    int oldPid = m_currentRunning->m_pid;

    const int cpu = peu->GetObject<CPU>()->GetId();
	if(!this->peu->hwModel->cpus[cpu]->inInterrupt)
		if(!m_currentRunning->m_currentProcessing.done)
			m_currentRunning->PreEmpt();

	m_currentRunning = m_threads[new_pid];

    NS_ASSERT(oldPid != new_pid);

    NS_LOG_INFO("Context switch " << oldPid << " -> " << new_pid); 

	// Do nothing if we are in an interrupt, because
	// this will be done by the interrupt-controller
	// upon entry/exit of handling interrupts
	if(!this->peu->hwModel->cpus[cpu]->inInterrupt)
		Simulator::ScheduleNow(&Thread::Dispatch, m_threads[new_pid]);

//	std::cout << "------------- CONTEXT SWITCH: " << oldPid << " -> " << m_currentRunning->m_pid << std::endl;
#endif
}

Ptr<Thread>
TaskScheduler::Fork(std::string threadName,
		Program *program,
		int priority,
		Ptr<Packet> currentPacket,
		std::map<std::string, Ptr<StateVariable> > localVars,
		std::map<std::string, Ptr<StateVariableQueue> > localStateQueues,
		bool infinite) {
  Ptr<Thread> t = CreateObject<Thread> ();
  t->peu = peu;
  t->SetScheduler(peu->taskScheduler);

  // Create a new program location, and
  // push onto the new thread's stack
  Ptr<ProgramLocation> pl = Create<ProgramLocation>();
  pl->program = program;
  pl->currentEvent = -1;
  pl->curPkt = currentPacket;
  pl->localStateVariableQueues = localStateQueues;
  pl->localStateVariables = localVars;


  // Generate infinite loop if requested
  if(infinite) {
	  pl->lc = new LoopCondition();
	  pl->lc->maxIterations = 0;
  }

  t->m_programStack.push(pl);

  // OYSTEDAL: 
  //
  // A fork may cause a reschedule, which might even update more than one core.
  // A rough sketch of the algo would be:
  // - Get the previously running pids
  // - Perform fork
  // - Perform any preemptions that is provoked by the fork
#if 1 
  std::vector<int> previouslyRunning = DoCurrentRunning();

  int pid = DoFork(priority);
  threadPids[threadName] = pid;

  t->SetPid(pid);
  m_threads[pid] = t;

  if (m_threads.size() <= NUM_CPU) {
      // t->Dispatch();
  } else {
      CheckAllCPUsForPreemptions(0, previouslyRunning);
  }
#else
  // A fork may provoke a task switch, if the
  // current running is the idle thread or the currently
  // running thread has consumed its time slice. Therefore,
  // we want to check whether the current running
  // changes during the fork.
  int crBeforeFork = this->m_currentRunning->GetPid();

  // Call DoFork. For CPUs, a SchedSim should
  // perform this task. For PEUs, this returns the first not-active thread
  // in its bitmap.
  int pid = DoFork(priority);
  threadPids[threadName] = pid;

  NS_LOG_INFO("Created thread " << threadName << " with pid " << pid); 

  t->SetPid(pid);
  m_threads[pid] = t;

  // OYSTEDAL: Idle threads should be dispatched immediately for all cores. 

  // If this is the first thread (should be the
  // idle thread), dispatch it. Otherwise, we may
  // have provoked a context switch, so check for this.
  if(m_threads.size() == 1) {
	    m_currentRunning = t;
	    t->Dispatch();
  } else if (this->DoCurrentRunning() != crBeforeFork) {
		m_currentRunning->PreEmpt();
	    m_currentRunning = t;
	    t->Dispatch();
  }
#endif

  return t;
}


void *TaskScheduler::AllocateTempSynch(int type, std::vector<uint32_t> arguments){
	return DoAllocateTempSynch(type,arguments);
}



////////////
// The following should be implemented by the SchedSim-specific sub-class
////////////

void* TaskScheduler::DoAllocateTempSynch(int type, std::vector<uint32_t> arguments){
	  NS_LOG_ERROR("DoAllocateTempSynch not specialized by SchedSim wrapper");
	  return 0;
}

void TaskScheduler::DoDeallocateTempSynch(void* var) {
	  NS_LOG_ERROR("DoDeallocateTempSynch not specialized by SchedSim wrapper");
}

void
TaskScheduler::DoTerminate(void)
{
  // If we wind up here, no SchedSim is specified. This is the
  // case for PEUs other than the CPU.

  // Nothing to do.
}

void
TaskScheduler::DoInitialize()
{
  NS_LOG_ERROR("DoStart not specialized by SchedSim wrapper");
}

int
TaskScheduler::DoFork(int priority) {
  // If we wind up here, no SchedSim is specified. This is the
  // case for PEUs other than the CPU.

  // Nothing to do. Simply return 0, this value will not be
  // of importance.
  return 0;
}

void
TaskScheduler::DoHandleSchedulerEvent()
{
  NS_LOG_ERROR("DoHandleSchedulerEvent not specialized by SchedSim wrapper");
}

int
TaskScheduler::DoRequest(int cpu, int type, std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoRequest not specialized by SchedSim wrapper");
  return 0;
}

std::vector<int>
TaskScheduler::DoCurrentRunning(void)
{
  NS_LOG_ERROR("DoCurrentRunning not specialized by SchedSim wrapper");
  return std::vector<int>();
}

void
TaskScheduler::DoAllocateSynch(int type, std::string id, std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoAllocateSynch not implemented by SchedSim");
}

/* FOR INTERRUPTS */
void
InterruptScheduler::DoTerminate() {
  // Here, the PEU must be the cpu
  peu->hwModel->m_interruptController->Proceed(peu->GetObject<CPU>()->GetId());
}

ParallelThreadsScheduler::ParallelThreadsScheduler()
{
  // Do nothing for now
}

NS_OBJECT_ENSURE_REGISTERED (ParallelThreadsScheduler);

/* FOR (current models of) PEUs */
TypeId 
ParallelThreadsScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::processing::ParallelThreadsScheduler")
    .SetParent<TaskScheduler> ()
    .AddConstructor<ParallelThreadsScheduler> ()
    .AddAttribute("maxthreads", "Maximum number of threads running in parallel",
                  UintegerValue(1),
		  MakeUintegerAccessor(&ParallelThreadsScheduler::m_maxThreads),
		  MakeUintegerChecker<uint32_t> ())
    ;

 return tid;
}

void
ParallelThreadsScheduler::Initialize(Ptr<PEU> nonCPUPEU)
{
  // Initialize bitmap
  // Create N threads according to attribute

//  std::cout << " IIIIIIIIIIIII " << std::endl;

  for(unsigned int i = 0; i < m_maxThreads; i++) {
    Ptr<Thread> t = CreateObject<Thread> ();
    t->SetPid(i);
    m_threads[i] = t;
  } 
}

// We override Terminate, as threads should not
// be removed, only be flagged as no longer active.
// Can only be called by the running thread itself.
void
ParallelThreadsScheduler::Terminate(unsigned int pid)
{
  m_freeList.push_back(pid);
}

// Returns NULL if no free thread, a smart pointer to thread elsewise.
// Priority is not used, but must be there to override the base class
// version of this function
Ptr<Thread>
ParallelThreadsScheduler::Fork(
		std::string threadName,
		Program *program,
		int priority,
		Ptr<Packet> currentPacket,
		std::map<std::string, Ptr<StateVariable> > localVars,
		std::map<std::string, Ptr<StateVariableQueue> > localStateQueues,
		bool infinite)
{
  // If no more threads are free, return
  if (m_freeList.empty())
    return NULL;

  // Find first non-active thread
  unsigned int freePid = m_freeList.front();
  Ptr<Thread> t = m_threads[freePid];

  // Remove this thread from the free list
  m_freeList.pop_front();

  // Create a new program location, and
  // push onto the new thread's stack
  Ptr<ProgramLocation> pl;
  pl->program = program;
  pl->currentEvent = 0;
  pl->curPkt = currentPacket;
  t->m_programStack.push(pl);
  pl->localStateVariables = localVars;
  pl->localStateVariableQueues = localStateQueues;

  // Dispatch thread
  t->Dispatch();

  return t;
}

} // namespace ns3
