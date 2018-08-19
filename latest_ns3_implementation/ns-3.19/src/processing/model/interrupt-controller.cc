#include "hwmodel.h"
#include "membus.h"
#include "interrupt-controller.h"
#include "peu.h"
#include <iterator>
#include <string>
#include <vector>
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InterruptController");

TypeId
InterruptController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InterruptController")
    .SetParent<Object> ()
    .AddConstructor<InterruptController> ()
  ;
  return tid;
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(std::string &s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

int getInterruptNumber(std::string str) {
	std::vector<std::string> parts = split(str, '-');
	// Fetch the interrupt number
	std::istringstream i(parts[1]);
	int interruptNumber;
    if(!(i >> interruptNumber)) {
		NS_FATAL_ERROR("Unable to obtain interrupt number from " << str << std::endl);
        return -1;
    }
    return interruptNumber;
}



void
InterruptController::IssueInterruptWithService(Ptr<SEM> intSem, struct tempVar tempsynch, Ptr<Packet> current,
		std::map<std::string, Ptr<StateVariable> > localStateVariables,
		std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues)
{
    NS_ASSERT(0);
    static int cpu = 0;
    ProgramLocation *p = new ProgramLocation;
    p->tempvar = tempsynch;
    p->curPkt = current;
    p->localStateVariables = localStateVariables;
    p->localStateVariableQueues = localStateVariablesQueues;

    IssueInterruptWithServiceOnCPU(cpu, intSem, p);
    // cpu = (cpu + 1) % 2;
}

void
InterruptController::IssueInterruptWithServiceOnCPU(int cpu, Ptr<SEM> intSem, Ptr<ProgramLocation> programLoc)
{
    // See: Understanding the Linux kernel, page 138
    //std::cout << "IssueInterruptWithServiceOnCPU" << std::endl;
#if 0

    int irq = getInterruptNumber(intSem->name);

    // spin lock?

    pending[irq] = true;

    // If this IRQ is serviced by another CPU, let that CPU do the next iteration as well
    if (!in_progress[irq]) {
        in_progress[irq] = true;

        // Create an interrupt request
#if 0
        InterruptRequest ir;
        ir.current = programLoc->curPkt;
        ir.service = intSem;
        ir.interruptNr = irq;
        ir.toCall = NULL;
        ir.tempsynch = programLoc->tempvar;
        ir.localStateVariables = programLoc->localStateVariables;
        ir.localStateVariablesQueues = programLoc->localStateVariableQueues;
#else
        InterruptRequest ir;
        ir.current = Ptr<Packet>();
        ir.service = intSem;
        ir.interruptNr = irq;
        ir.toCall = NULL;
        ir.tempsynch = tempVar();
        ir.localStateVariables = std::map<std::string, Ptr<StateVariable> >();
        ir.localStateVariablesQueues = std::map<std::string, Ptr<StateVariableQueue> >();
#endif

        if(currentlyHandled[cpu].interruptNr == -1)
        {
            currentlyHandled[cpu] = ir;
            hwModel->cpus[cpu]->Interrupt(ir);
        }
        else {
            if(pendingRequests[cpu].size() < queueSize)
                pendingRequests[cpu].push(ir);
        }
    }
#else
    if (intSem == NULL)
      NS_FATAL_ERROR("Interrupt not located" << std::endl);
    std::vector<std::string> parts = split(intSem->name, '-');

    // Fetch the interrupt number
    std::istringstream i(parts[1]);
    int interruptNumber;
    if(!(i >> interruptNumber))
        NS_FATAL_ERROR("Unable to obtain interrupt number from " << intSem->name << std::endl);

    /*
    struct tempVar tempsynch = programLoc->tempvar;
    Ptr<Packet> current = programLoc->curPkt;
    std::map<std::string, Ptr<StateVariable> > localStateVariables = programLoc->localStateVariables;
    std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues = programLoc->localStateVariableQueues;
    */


    // If already pending, simply return. The'
    // interrupt is in that case lost.
    if(pending[interruptNumber] || masked[interruptNumber])
        return;

    NS_LOG_INFO("Interrupt " << interruptNumber << " on CPU" << cpu);

    // Create an interrupt request
    /*
    InterruptRequest ir;
    ir.current = current;
    ir.service = intSem;
    ir.interruptNr = interruptNumber;
    ir.toCall = NULL;
    ir.tempsynch = tempsynch;
    ir.localStateVariables = localStateVariables;
    ir.localStateVariablesQueues = localStateVariablesQueues;
    */

    InterruptRequest ir;
    ir.current = programLoc->curPkt;
    ir.service = intSem;
    ir.interruptNr = interruptNumber;
    ir.toCall = NULL;
    ir.tempsynch = programLoc->tempvar;
    ir.localStateVariables = programLoc->localStateVariables;
    ir.localStateVariablesQueues = programLoc->localStateVariableQueues;

    // If no interrupt is currently handled,
    // we issue an interrupt to the CPU.
    // On the CPU, interrupts may or may not
    // be enabled, but whey they are enabled
    // the pending interrupt will be executed,
    // and on completion the ExecuteNext function
    // will be called.
    if(currentlyHandled[cpu].interruptNr == -1)
    {
        currentlyHandled[cpu] = ir;
        hwModel->cpus[cpu]->Interrupt(ir);
        // cpu = (cpu+1) % 2;
    }
    else {
        if(pendingRequests[cpu].size() < queueSize)
            pendingRequests[cpu].push(ir);
    }
#endif
}

#if 0
void
InterruptController::IssueInterruptWithService(Ptr<SEM> intSem, struct tempVar tempsynch, Ptr<Packet> current,
		std::map<std::string, Ptr<StateVariable> > localStateVariables,
		std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues)
{
	std::vector<std::string> parts = split(intSem->name, '-');

	// Fetch the interrupt number
	std::istringstream i(parts[1]);
	int interruptNumber;
	if(!(i >> interruptNumber))
		NS_FATAL_ERROR("Unable to obtain interrupt number from " << intSem->name << std::endl);

  // If already pending, simply return. The'
  // interrupt is in that case lost.
  if(pending[interruptNumber] || masked[interruptNumber])
    return;

  // Create an interrupt request
  InterruptRequest ir;
  ir.current = current;
  ir.service = intSem;
  ir.interruptNr = interruptNumber;
  ir.toCall = NULL;
  ir.tempsynch = tempsynch;
  ir.localStateVariables = localStateVariables;
  ir.localStateVariablesQueues = localStateVariablesQueues;

  // If no interrupt is currently handled,
  // we issue an interrupt to the CPU.
  // On the CPU, interrupts may or may not
  // be enabled, but whey they are enabled
  // the pending interrupt will be executed,
  // and on completion the ExecuteNext function
  // will be called.
  if(currentlyHandled.interruptNr == -1)
    {
      currentlyHandled = ir;
      static int cpu = 0;
      hwModel->cpus[cpu]->Interrupt(ir);
      // cpu = (cpu+1) % 2;
    }
  else {
    if(pendingRequests.size() < queueSize)
      pendingRequests.push(ir);
  }
}
#endif

void
InterruptController::IssueInterrupt(int interruptNumber, std::string service, Ptr<Packet> current)
{
    NS_ASSERT(0);
  // If already pending, simply return. The'
  // interrupt is in that case lost.
  if(pending[interruptNumber])
    return;

  // Create an interrupt request
  InterruptRequest ir;
  ir.current = current;
  ir.serviceString = service;
  ir.interruptNr = interruptNumber;
  ir.toCall = NULL;

  // If no interrupt is currently handled,
  // we issue an interrupt to the CPU.
  // On the CPU, interrupts may or may not
  // be enabled, but whey they are enabled
  // the pending interrupt will be executed,
  // and on completion the ExecuteNext function
  // will be called.
  if(currentlyHandled[0].interruptNr == -1)
    {
      currentlyHandled[0] = ir;
      hwModel->cpus[0]->Interrupt(ir);
    }
  else {
    if(pendingRequests[0].size() < queueSize)
      pendingRequests[0].push(ir);
  }
}

void
InterruptController::IssueInterruptNoProcessing(int interruptNumber, EventImpl *callback)
{
  // If allready pending, simply return. The'
  // interrupt is in that case lost.
  if(pending[interruptNumber])
    return;

  // Create an interrupt request
  InterruptRequest ir;
  ir.interruptNr = interruptNumber;
  ir.toCall = callback;

  // If no interrupt is currently handled,
  // we issue an interrupt to the CPU.
  // On the CPU, interrupts may or may not
  // be enabled, but whey they are enabled
  // the pending interrupt will be executed,
  // and on completion the ExecuteNext function
  // will be called.
  if(currentlyHandled[0].interruptNr == -1)
    {
      currentlyHandled[0] = ir;
      ir.toCall->Invoke();
      ir.toCall->Unref();
      Proceed(0);
    }
  else {
    if(pendingRequests[0].size() < queueSize)
      pendingRequests[0].push(ir);
  }
}

// If we have more interrupt, call the
// interrupt routine of the CPU at once.
// The CPU will call the Dispatch of the
// correct program 
void
InterruptController::Proceed(int cpu)
{
  if(pendingRequests[cpu].empty()) {
    in_progress[currentlyHandled[cpu].interruptNr] = false;
    currentlyHandled[cpu].interruptNr = -1;

    // OYSTEDAL: Continue the execution of whatever was executing on the
    // interrupted core.

    // Simulator::ScheduleNow(&Thread::Dispatch, hwModel->cpus[cpu]->taskScheduler->m_currentRunning);
    Simulator::ScheduleNow(&Thread::Dispatch, hwModel->cpus[cpu]->taskScheduler->GetCurrentRunningThread(cpu));
  }
  else {
    currentlyHandled[cpu] = pendingRequests[cpu].front();
    pendingRequests[cpu].pop();
    
    // Check if we have a processing interrupt or not
    if(currentlyHandled[cpu].toCall != NULL) {
      currentlyHandled[cpu].toCall->Invoke();
      currentlyHandled[cpu].toCall->Unref();

      // We can afford recursiveness here, as it is
      // unlikely that we have many waiting interruptss
      // causing the stack to overflow.
      Proceed(cpu);
    }
    else {
      Ptr<Packet> currentPacket = currentlyHandled[cpu].current;
      hwModel->cpus[cpu]->Interrupt(currentlyHandled[cpu]);
    }
  }
}

InterruptController::InterruptController (void)
{
  currentlyHandled[0] = InterruptRequest();
  currentlyHandled[1] = InterruptRequest();
  currentlyHandled[0].interruptNr = -1;
  currentlyHandled[1].interruptNr = -1;
  currentlyHandled[0].tempsynch = tempVar();
  currentlyHandled[1].tempsynch = tempVar();

  int i = 0;
  for(; i < NUMBER_OF_INTERRUPTS; i++) {
	  masked[i] = false;
	  pending[i] = false;
      in_progress[i] = false;
  }

  queueSize = DEFAULT_QUEUE_SIZE;
}

}
