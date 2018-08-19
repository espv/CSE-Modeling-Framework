#include "apic.h"

namespace ns3 {

TypeId
APIC::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::APIC")
    .SetParent<InterruptController> ()
    .AddConstructor<APIC> ()
  ;
  return tid;
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(std::string &s, char delim);

/*
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
*/

void
APIC::IssueInterruptWithService(Ptr<SEM> intSem, struct tempVar tempsynch, Ptr<Packet> current,
		std::map<std::string, Ptr<StateVariable> > localStateVariables,
		std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues)
{
    // static int cpu = 0;
    IssueInterruptWithServiceOnCPU(0, intSem, tempsynch, current, localStateVariables, localStateVariablesQueues);
}

void
APIC::IssueInterruptWithServiceOnCPU(int cpu, Ptr<SEM> intSem, struct tempVar tempsynch, Ptr<Packet> current,
		std::map<std::string, Ptr<StateVariable> > localStateVariables,
		std::map<std::string, Ptr<StateVariableQueue> > localStateVariablesQueues)
{
    // See: Understanding the Linux kernel, page 138

    int irq;
    if((irq = getInterruptNumber(intSem->name)) < 0) return;

    // spin lock?

    pending[irq] = true;

    // If this IRQ is serviced by another CPU, let that CPU do the next iteration as well
    if (!in_progress[irq]) {
        in_progress[irq] = true;

        // Create an interrupt request
        InterruptRequest ir;
        ir.current = current;
        ir.service = intSem;
        ir.interruptNr = irq;
        ir.toCall = NULL;
        ir.tempsynch = tempsynch;
        ir.localStateVariables = localStateVariables;
        ir.localStateVariablesQueues = localStateVariablesQueues;

        if(currentlyHandled[cpu].interruptNr == -1)
        {
            currentlyHandled[cpu] = ir;
            hwModel->cpus[cpu]->Interrupt(ir);
        }
        else {
            if(pendingRequests[cpu].size() < 256)
                pendingRequests[cpu].push(ir);
        }
    }
#if 0
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
#endif
}

APIC::APIC() : InterruptController() {
    for(int j = 0; j < NUMBER_OF_INTERRUPTS; j++) {
        pending[j] = false;
        in_progress[j] = false;
    }

    for(int i = 0; i < NUM_CPUS; i++) {
        currentlyHandled[i].interruptNr = -1;
    }
}

}
