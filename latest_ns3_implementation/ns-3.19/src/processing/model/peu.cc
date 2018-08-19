#include "ns3/object.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fstream>

#include "peu.h"
#include "program.h"
#include "thread.h"
#include "hwmodel.h"
#include "membus.h"
#include "interrupt-controller.h"
#include "execenv.h"
#include "sem.h"

#include <string>

namespace ns3 {

TypeId PEU::GetTypeId(void) {
	static TypeId tid =
			TypeId("ns3::processing::PEU")
            .SetParent<Object>().AddConstructor<PEU>()
            .AddAttribute("frequency", "Frequency of the unit", UintegerValue(100), MakeUintegerAccessor(&PEU::m_freq), MakeUintegerChecker<uint32_t>())
            .AddAttribute("name", "Identifying name of the unit", StringValue(""), MakeStringAccessor(&PEU::m_name), MakeStringChecker()).AddAttribute(
					"traceoverhead",
					"The average number of cycles spent per call to the tracing function",
					UintegerValue(300),
					MakeUintegerAccessor(&PEU::m_tracingOverhead),
					MakeUintegerChecker<uint32_t>());
	return tid;
}

PEU::PEU() :
		m_freq(0) {
}

PEU::PEU(std::string name, long freq) :
		m_name(name), m_freq(freq) {
}

PEU::~PEU() {
}

void PEU::Consume(ProcessingInstance *pi) {
	// Iterate all resources. We determine the
	// consumption according to the following
	// dependencies:
	//
	// duration = cycles / frequency
	// cycles = instructions +
	// CACHE MISSES + CACHE LINE SIZE = MEMORY ACCESSES
	// MEMORY CYCLES + MEMORY FREQUENCY = MEMORY DURATION
	// MEMORY ACCESSES + CONTENTION = MEMORY CYCLES
	// INSTRUCTIONS + MEMORY DURATION = PEU CYCLES
	// PEU CYCLES + PEU FREQUENCY = PEU DURATION
	// TOTAL DURATION = PEU DURATION + MEMORY DURATION
	//
	// The loop below will iterate the resources and
	// add entities from level n to level n - 1 for
	// each iteration. When an entity at level 1 is
	// discovered (nanoseconds), we return.
	while (!pi->remaining[NANOSECONDS].defined) {
		// If we are provided a set of NANOSECONDS, simply return -
		// we don't have anything to calculate.
		if (pi->remaining[NANOSECONDS].defined)
			break;

		// If we are provided with a set of cycles, calculate
		// duration by means of dividing by frequency
		else if (pi->remaining[CYCLES].defined) {
			pi->remaining[NANOSECONDS].amount = ((pi->remaining[CYCLES].amount
					* 1000) / m_freq) - ((m_tracingOverhead * 1000) / m_freq);
			if (pi->remaining[NANOSECONDS].amount < 0.0)
				pi->remaining[NANOSECONDS].amount = 0;

			pi->remaining[NANOSECONDS].defined = true;
		}

		// If we have neither duration nor cycles defined, we
		// must have been provided with instructions and
		// cache misses or memory accesses
		else {
			// If we have been provided with cache misses,
			// calculate and store memory accesses
			if (pi->remaining[CACHEMISSES].defined)
				pi->remaining[MEMORYACCESSES].amount =
						pi->remaining[CACHEMISSES].amount
								* hwModel->cacheLineSize;

			// Here, we must have both instructions and
			// memory accesses defined. If not, complain!
			NS_ASSERT(
					pi->remaining[INSTRUCTIONS].defined
							&& pi->remaining[MEMORYACCESSES].defined);

			// TODO: we do not contend for now - one of the next steps if necessary.
			hwModel->m_memBus->Contend(pi); // TODO: UPDATE freq TO NANOSECONDS INSTEAD!
			pi->remaining[NANOSECONDS].amount =
					pi->remaining[INSTRUCTIONS].amount / m_freq
							+ pi->remaining[MEMSTALLCYCLES].amount
									/ hwModel->m_memBus->freq;
		}
	}

	// Finally, schedule the completion function to run
	// after the computed duration.

    double amount = pi->remaining[NANOSECONDS].amount * pi->factor;
	pi->processingCompleted = Simulator::Schedule(
            NanoSeconds(amount),
			&Thread::DoneProcessing, pi->thread);
}

/***************************************************/
/*********************** CPU: **********************/
/***************************************************/

TypeId CPU::GetTypeId(void) {
	static TypeId tid =
			TypeId("ns3::processing::CPU").SetParent<PEU>().AddConstructor<CPU>();
	return tid;
}

void CPU::EnableInterrupts() {
	interruptsEnabled = true;
}

extern std::map<int, int> m_fifo_debugfiles;
extern bool debugOn;

CPU::CPU() {
	interruptsEnabled = true;
	inInterrupt = false;

	// For interrupt debugging. -1 indicates we are in an interrupt (see Thread::Dispatch()).
	std::ostringstream stringStream;
	stringStream << "/tmp/ns3leu" << -1;
	m_fifo_debugfiles[-1] = open(stringStream.str().c_str(),
			O_WRONLY | O_NONBLOCK);
//  int result = m_fifo_debugfiles[-1];
//  std::cout << result << std::endl;
}

int CPU::GetId() {
    // TODO: Do this once at init instead of at every call
    int id;
    // Extract cpuid from name (cpu0, cpu1)
    std::string name = PEU::m_name;
    name.erase(name.begin(), name.begin()+3);
    std::istringstream i(name);
    i >> id;

    return id;
}

void CPU::DisableInterrupt() {
	interruptsEnabled = false;
}

bool CPU::Interrupt(InterruptRequest ir) {
	// Interrupt models
	// are simply present as SEMs (format "interrupt::nr").
	// When they are executed, we (1) disable interrupts,
	// (2) instantiate the program from the SEM, (3)
	// execute the program, (4) Issue an End of Interrupt
	// (5) If no more interrupts were ready, we enable interrupts and
	// (6) check if a context switch is imminent. For
	// now, we simply drop interrupts that occur during
	// disabled interrupts.

	if (!interruptsEnabled)
		return false;
	else {
		Program *newProgram = ir.service->rootProgram;

		// Indicate to users of synchronization primitives and
		// the task scheduler that we are in an interrupt, and
		// no dispatching should occur. This is because the
		// interrupt controller dispatches the currently running
		// thread when it is finished handling interrupts.
		inInterrupt = true;

		// Create new thread of execution
		Ptr<ProgramLocation> newProgramLocation = Create<ProgramLocation>();
		newProgramLocation->program = newProgram;
		newProgramLocation->currentEvent = -1;
		newProgramLocation->tempvar = ir.tempsynch;
		newProgramLocation->curPkt = ir.current;
		newProgramLocation->localStateVariables = ir.localStateVariables;
		newProgramLocation->localStateVariableQueues =
				ir.localStateVariablesQueues;

		Ptr<ExecEnv> ee = this->hwModel->node->GetObject<ExecEnv>();

        NS_ASSERT_MSG(ee->serviceQueues[this->hirqQueue],
                "Unable to find interrupt service queue named " << this->hirqQueue);

		ee->serviceQueues[this->hirqQueue]->push(
				std::pair<Ptr<SEM>, Ptr<ProgramLocation> >(ir.service,
						newProgramLocation));

        NS_ASSERT_MSG(ee->serviceQueues[this->hirqQueue],
                "Unable to find interrupt handler named " << this->hirqHandler);

		Ptr<ProgramLocation> irqProgramLocation = Create<ProgramLocation>();
		irqProgramLocation->program =
				ee->m_serviceMap[this->hirqHandler]->rootProgram;
		irqProgramLocation->currentEvent = -1;
		irqProgramLocation->tempvar = ir.tempsynch;
		irqProgramLocation->curPkt = ir.current;
		irqProgramLocation->localStateVariables = ir.localStateVariables;
		irqProgramLocation->localStateVariableQueues =
				ir.localStateVariablesQueues;

		interruptThread->m_programStack.push(irqProgramLocation);

        // OYSTEDAL: If there's a thread running on the interrupted core, we 
        // need to preempt it unless it's already done processing.
        //
		// if (!taskScheduler->m_currentRunning->m_currentProcessing.done)
			// taskScheduler->m_currentRunning->PreEmpt();
        Ptr<Thread> cr = taskScheduler->GetCurrentRunningThread(GetId());
        if (!cr->m_currentProcessing.done)
            cr->PreEmpt();

        interruptThread->peu = this;

		// Schedule execution of interrupt now
		Simulator::ScheduleNow(&Thread::Dispatch, interruptThread);

		return true;
	}
}

}
