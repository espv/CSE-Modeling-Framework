#include "ns3/object.h"
#include "execenv.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

#include "hwmodel.h"
#include "program.h"
#include "peu.h"
#include "membus.h"
#include "sem.h"
#include "interrupt-controller.h"
#include "condition.h"
// #include "ns3/schedsim-linsched.h"
#include "ns3/rrscheduler.h"
#include <ns3/drop-tail-queue.h>
#include "ns3/local-state-variable-queue.h"

#include "apic.h"

#include <iostream>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ExecEnv");
NS_OBJECT_ENSURE_REGISTERED(ExecEnv);

// TODO OYSTEDAL: Move to separate util file?
bool is_prefix(std::string prefix, std::string str) {
    return std::mismatch(prefix.begin(), prefix.end(), str.begin()).first == prefix.end();
}

TypeId ExecEnv::GetTypeId(void) {
	static TypeId tid =
			TypeId("ns3::ExecEnv").SetParent<Object>().AddConstructor<ExecEnv>();
	return tid;
}

ExecEnv::ExecEnv() :
		m_traceOverhead(0) {
}

void ExecEnv::Initialize(std::string device) {
	// Create hardware model
	Ptr<HWModel> hwMod = CreateObject<HWModel>();
	hwModel = hwMod;

	// Create conditions object
	conditionFunctions = CreateObject<ConditionFunctions>();
	conditionFunctions->Initialize(GetObject<ExecEnv>());

	// Create interrupt controller for the hwModel
#if 0
    Ptr<InterruptController> ic = CreateObject<APIC>();
    hwModel->m_interruptController = ic;
	hwModel->node = GetObject<Node>();
	ic->hwModel = hwModel;
#else
	Ptr<InterruptController> ic = CreateObject<InterruptController>();
	hwModel->m_interruptController = ic;
	hwModel->node = GetObject<Node>();
	ic->hwModel = hwModel;
#endif

	// Parse device file to create the rest
	Parse(device);
}

void ExecEnv::HandleQueue(std::vector<std::string> tokens) {
	// Go throught types (currently only support FIFO)
	if (!tokens[1].compare("FIFO")) {

		// If we have a service queue, we don't need to do anything for now.
		// Its using a standard C++ map, which will initialize the elements
		// as they are being used
		if (!tokens[3].compare("services")) {
			//std::cout << "Encountered service FIFO queue" << tokens[0] << std::endl;
			// Add empty vector (just to set the key)
			this->serviceQueues[tokens[0]] = new std::queue<
					std::pair<Ptr<SEM>, Ptr<ProgramLocation> > >();

			// Add the queue to the end of queueOrder. This is used
			// by conditions ("and" loops; they are really conditions
			// as well).
			serviceQueueOrder.push_back(serviceQueues[tokens[0]]);

			// Add the queue to the reverse mapping
			serviceQueueNames[serviceQueues[tokens[0]]] = tokens[0];

			return;
		} else if (!tokens[3].compare("states")) {
			stateQueueOrder.push_back(tokens[0]);
			if (!tokens[4].compare("global")) {
				stateQueues[tokens[0]] = Create<StateVariableQueue>();
				stateQueueNames[stateQueues[tokens[0]]] = tokens[0];
			}
		}

		// We have a packet queue.
		//
		// No size = no upper bound on contents
		else if (!tokens[2].compare("-1"))
			queues[tokens[0]] = CreateObjectWithAttributes<DropTailQueue>(
					"MaxPackets", UintegerValue(4294967295));

		// We have a size
		else {
			// Get size
			std::istringstream i(tokens[2]);
			int size;
			if (!(i >> size))
				NS_FATAL_ERROR(
						"Unable to convert queue size " << tokens[2] << " to integer" << std::endl);
			// Act according to units
			if (!tokens[3].compare("packets")) {
				queues[tokens[0]] = CreateObjectWithAttributes<DropTailQueue>(
						"MaxPackets", UintegerValue(size));
			} else {
				queues[tokens[0]] = CreateObjectWithAttributes<DropTailQueue>(
						"MaxBytes", UintegerValue(size));
			}
		}
	} else {
		NS_FATAL_ERROR("Unsupported queue type " << tokens[1]);
		exit(1);
	}

	// Add the queue to the end of queueOrder. This is used
	// by conditions ("and" loops; they are really conditions
	// as well).
	queueOrder.push_back(queues[tokens[0]]);

	// Add the queue to the reverse mapping
	queueNames[queues[tokens[0]]] = tokens[0];
}

void ExecEnv::HandleSynch(std::vector<std::string> tokens) {
	// Get the name
	std::string name = tokens[0];

	// Get the type
	unsigned int type = stringToUint32(tokens[1]);

	// get arguments by iteration the rest of the line
	unsigned int j = 2;
	uint32_t argvalue;
	std::vector<uint32_t> arguments;
	for (; j < tokens.size(); j++) {
		argvalue = stringToUint32(tokens[j]);
		arguments.push_back((uint64_t) argvalue);
		//std::cout << argvalue << " ";
	}

	//std::cout << std::endl;

	// Allocate the synch primitive
	hwModel->cpus[0]->taskScheduler->AllocateSynch(type, name, arguments);
}

void ExecEnv::HandleThreads(std::vector<std::string> tokens) {
	Program *program = m_serviceMap[tokens[1]]->rootProgram;

	// Create an infinite loop around the root program if
	// so is specified
	bool infinite = !tokens[2].compare("infinite");
	m_serviceMap[tokens[1]]->peu->taskScheduler->Fork(tokens[0], program,
			stringToUint32(tokens[3]), NULL,
			std::map<std::string, Ptr<StateVariable> >(),
			std::map<std::string, Ptr<StateVariableQueue> >(), infinite);
}

void ExecEnv::HandleHardware(std::vector<std::string> tokens) {
	// get type, name, frequency (Mhz) and scheduler (if peu)
	std::istringstream i;

	// Parse frequency
	i.str(tokens[1]);
	int freq;
	if (!(i >> freq)) {
		NS_FATAL_ERROR("Unable to parse frequency " << tokens[1] << std::endl);
		exit(1);
	}

	// Act accordint to type
	// If we have a membus, create and install in hardware
	if (!tokens[0].compare("MEMBUS")) {
		// Create memory bus for the hwModel
		Ptr<MemBus> membus = CreateObjectWithAttributes<MemBus>("frequency",
				UintegerValue(freq));
		hwModel->m_memBus = membus;
	}

	// If we have a PEU, create and install.
	else if (!tokens[0].compare("PEU")) {
		Ptr<PEU> newPEU;

		// Treat the name CPU specially. As it is the only PEU that
		// can be intteruted, is has its own type and member variable in hwModel.
		ObjectFactory factory;
		factory.SetTypeId(tokens[3]);
		// if (!tokens[2].compare("cpu")) {
        if (is_prefix("cpu", tokens[2])) { // OYSTEDAL
			uint32_t tracingOverhead = 0;
			std::istringstream j;
			j.str(tokens[4]);
			if (!(j >> tracingOverhead)) {
				NS_FATAL_ERROR(
						"Unable to parse tracing overhead " << tokens[4] << std::endl);
				exit(1);
			}

			newPEU = CreateObjectWithAttributes<CPU>("frequency",
					UintegerValue(freq), "name", StringValue(tokens[2]),
					"traceoverhead", UintegerValue(tracingOverhead));

			// hwModel->cpus = newPEU->GetObject<CPU>();
            Ptr<CPU> cpu = newPEU->GetObject<CPU>();
            hwModel->AddCPU(cpu);
			cpu->hirqHandler = tokens[5];
			cpu->hirqQueue = tokens[6];

            newPEU->hwModel = hwModel;

            // static Ptr<TaskScheduler> ts = NULL;
            if (cpuScheduler == NULL) {
                cpuScheduler = factory.Create()->GetObject<RoundRobinScheduler>();
                cpuScheduler->Initialize(newPEU);
            }

			newPEU->taskScheduler = cpuScheduler;
					
			cpu->interruptThread = CreateObject<Thread>();
			cpu->interruptThread->peu = newPEU;
			cpu->interruptThread->m_scheduler = newPEU->taskScheduler;
		}

		// All other types of PEUs are treated the same
		else {
			newPEU = CreateObjectWithAttributes<PEU>("frequency",
					UintegerValue(freq), "name", StringValue(tokens[2]));
			hwModel->m_PEUs[tokens[3]] = newPEU;
			newPEU->taskScheduler =
                    factory.Create()->GetObject<RoundRobinScheduler>();
            newPEU->hwModel = hwModel;
            newPEU->taskScheduler->Initialize(newPEU);
		}

		// Set the taskscheduler via an object factory
	}
}

void ExecEnv::HandleConditions(std::vector<std::string> tokens) {
	// Format: TYPE, location/name,[ scope if not loop,] condition name[, condition name 1, ..., condition name n-1 if packet characteristics]
	if (!tokens[0].compare("VCLOC")) {
		locationConditions[tokens[1]].condName = tokens[3];
		locationConditions[tokens[1]].scope =
				tokens[2].compare("global") ? CONDITIONLOCAL : CONDITIONGLOBAL;
	} else if (!tokens[0].compare(
			"ENQUEUE")/* || !tokens[0].compare("ENQUEUEN")*/) {
		struct condition newEnQCond;
		newEnQCond.condName = tokens[3];
		newEnQCond.scope =
				tokens[2].compare("global") ? CONDITIONLOCAL : CONDITIONGLOBAL;
		enqueueConditions[tokens[1]].push_back(newEnQCond);
	} else if (!tokens[0].compare(
			"DEQUEUE")/* || !tokens[0].compare("DEQUEUEN")*/) {
		struct condition newDeQCond;
		newDeQCond.condName = tokens[3];
		newDeQCond.scope =
				tokens[2].compare("global") ? CONDITIONLOCAL : CONDITIONGLOBAL;
		dequeueConditions[tokens[1]].push_back(newDeQCond);
	} else if (!tokens[0].compare("LOOP")) {
		// Note that for loops, we regard it to be sufficient with
		// only one codition function, because _it_ can work
		// internally with compound conditions.
		loopConditions[tokens[1]].condName = tokens[2];
		// A condition on a loop is allways local
		loopConditions[tokens[1]].scope = CONDITIONLOCAL;
	}
}

void ExecEnv::HandleTriggers(std::vector<std::string> tokens) {
	if (!tokens[0].compare("LOC")) {
		locationTriggers[tokens[1]] = tokens[2];
	} else if (!tokens[0].compare("SERVICE")) {
		serviceTriggers[tokens[1]] = tokens[2];
		triggerToRWFunc[tokens[2]] = tokens[1];
		RWFuncToTrigger[tokens[1]] = tokens[2];
	} else if (!tokens[0].compare("QUEUE")) {
		dequeueTriggers[tokens[1]] = tokens[2];
	}
}

bool ExecEnv::queuesIn(std::string first, std::string last, LoopCondition *lc) {
	if(lc->stateQueues) {
		std::vector<std::string>::iterator qIt = std::find(stateQueueOrder.begin(),
				stateQueueOrder.end(), first);
		std::vector<std::string>::iterator qItLast = std::find(
				stateQueueOrder.begin(), stateQueueOrder.end(), last);

		// Iterate through all queues in between according
		// to the queue order
		while (true) {
			if (std::find(lc->stateQueuesServed.begin(), lc->stateQueuesServed.end(),
					stateQueues[*qIt]) != lc->stateQueuesServed.end())
				return true;
			if (qIt == qItLast || qIt == stateQueueOrder.end())
				break;
			qIt++;
		}
	}
	else if (!lc->serviceQueues) {
		Ptr<Queue> firstQueue = queues[first];
		Ptr<Queue> lastQueue = queues[last];

		// Iterate queues in the queue order, and
		// search for each queue between and includingget
		// firstQueue and lastQueue in lc->queues.
		// Upon the first hit, set the
		// dequeueOrLoopEncountered boolean variable
		// to true.
		std::vector<Ptr<Queue> >::iterator qIt = std::find(queueOrder.begin(),
				queueOrder.end(), firstQueue);
		std::vector<Ptr<Queue> >::iterator qItLast = std::find(
				queueOrder.begin(), queueOrder.end(), lastQueue);

		// Iterate through all queues in between according
		// to the queue order
		while (true) {
			if (std::find(lc->queuesServed.begin(), lc->queuesServed.end(),
					*qIt) != lc->queuesServed.end())
				return true;
			if (qIt == qItLast || qIt == queueOrder.end())
				break;
			qIt++;
		}
	} else {
		std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *firstQueue =
				serviceQueues[first];
		std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *lastQueue =
				serviceQueues[last];

		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *>::iterator qIt =
				std::find(serviceQueueOrder.begin(), serviceQueueOrder.end(),
						firstQueue);
		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *>::iterator qItLast =
				std::find(serviceQueueOrder.begin(), serviceQueueOrder.end(),
						lastQueue);

		for (; qIt != qItLast; qIt++)
			if (std::find(lc->serviceQueuesServed.begin(),
					lc->serviceQueuesServed.end(), *qIt)
					!= lc->serviceQueuesServed.end())
				return true;

	}

	return false;
}

void ExecEnv::fillQueues(std::string first, std::string last,
		LoopCondition *lc) {
	// Assume first and last queues are of the same type
	if (!lc->serviceQueues && !lc->stateQueues) {
		Ptr<Queue> firstQueue = queues[first];
		Ptr<Queue> lastQueue = queues[last];

		// Iterate queues in the queue order, and
		// insert each queue between and including
		// firstQueue and lastQueue in lc->queues.
		std::vector<Ptr<Queue> >::iterator qIt = std::find(queueOrder.begin(),
				queueOrder.end(), firstQueue);
		std::vector<Ptr<Queue> >::iterator qItLast = std::find(
				queueOrder.begin(), queueOrder.end(), lastQueue);

		// Push all _except_ the last one
		for (; qIt != qItLast; qIt++)
			lc->queuesServed.push_back(*qIt);

		// Push the last one if we actually had any
		if (qIt != queueOrder.end())
			lc->queuesServed.push_back(*qIt);
	} else if (lc->stateQueues) {
		// Iterate queues in the queue order, and
		// insert each queue between and including
		// firstQueue and lastQueue in lc->queues.
		std::vector<std::string>::iterator qIt = std::find(
				stateQueueOrder.begin(), stateQueueOrder.end(), first);
		std::vector<std::string>::iterator qItLast = std::find(
				stateQueueOrder.begin(), stateQueueOrder.end(), last);

		// Push all _except_ the last one
		for (; qIt != qItLast; qIt++)
			lc->stateQueuesServed.push_back(stateQueues[*qIt]);

		// Push the last one if we actually had any
		if (qIt != stateQueueOrder.end())
			lc->stateQueuesServed.push_back(stateQueues[*qIt]);
	} else {
		std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *firstQueue =
				serviceQueues[first];
		std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > * lastQueue =
				serviceQueues[last];

		// Iterate queues in the queue order, and
		// insert each queue between and including
		// firstQueue and lastQueue in lc->queues.
		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *>::iterator qIt =
				std::find(serviceQueueOrder.begin(), serviceQueueOrder.end(),
						firstQueue);
		std::vector<std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *>::iterator qItLast =
				std::find(serviceQueueOrder.begin(), serviceQueueOrder.end(),
						lastQueue);

		// Push all _except_ the last one
		for (; qIt != qItLast; qIt++)
			lc->serviceQueuesServed.push_back(*qIt);

		// Push the last one if we actually had any
		if (qIt != serviceQueueOrder.end())
			lc->serviceQueuesServed.push_back(*qIt);
	}
}

double ExecEnv::stringToDouble(const std::string& s) {
	std::istringstream i(s);
	double x;
	if (!(i >> x)) {
		NS_FATAL_ERROR(
				"Could not convert string " << s << " to double" << std::endl);
		exit(1);
	}
	return x;
}

uint32_t ExecEnv::stringToUint32(const std::string& s) {
	std::istringstream i(s);
	uint32_t x;
	if (!(i >> x)) {
		NS_FATAL_ERROR(
				"Could not convert string " << s << " to uint32_t" << std::endl);
		exit(1);
	}

	return x;
}

std::string ExecEnv::deTokenize(std::vector<std::string> tokens) {
	std::string result = "";
	std::vector<std::string>::iterator it = tokens.begin();

	while (it != tokens.end()) {
		result.append(*it);
		result.append(" ");
		it++;
	}

	return result;
}

ProcessingStage ExecEnv::addProcessingStages(ProcessingStage a,
		ProcessingStage b) {
	ProcessingStage toReturn;
	// Run through resources in both processing stages, and sum them
	for (int i = 0; i < LASTRESOURCE; i++) {
		if (a.resourcesUsed[i].defined) {
			// Check that both a and b are the same wrt. this resource
			if (a.resourcesUsed[i].defined != b.resourcesUsed[i].defined
					|| a.resourcesUsed[i].distributionType.compare(
							b.resourcesUsed[i].distributionType)) {
				NS_FATAL_ERROR(
						"Attempted to add different types of distributions");
				exit(1);
			}

			// Else wise, compute the combined averages and standard deviations
			// NOTE: we currently only support normal distributions, this should be
			// extended to lognormal.
			// For normal
			if (!a.resourcesUsed[i].distributionType.compare("normal")) {
				// In normal distributions, param1 is average and param 2 the variance
				double m2a = a.resourcesUsed[i].param2 * a.samples;
				double m2b = b.resourcesUsed[i].param2 * b.samples;
				double avga = a.resourcesUsed[i].param1;
				double avgb = b.resourcesUsed[i].param1;
				double counta = a.samples;
				double countb = b.samples;
				double countx = counta + countb;

				// Sigma is required for the calculation of the combined distribution
				double sigma = avgb - avga;

				// Calculate combined distribution parameters based on sigma
				// and the input distribution parameters.
				double combavg = avga + sigma * (countb / countx);
				double combvar = m2a + m2b
						+ (sigma * sigma) * (counta * countb / countx);

				// Create distribution and store in toReturn
				toReturn.resourcesUsed[i].consumption = NormalVariable(combavg,
						combvar);
				toReturn.resourcesUsed[i].defined = true;
				toReturn.samples = countx;
				toReturn.resourcesUsed[i].param1 = combavg;
				toReturn.resourcesUsed[i].param2 = combvar;

			} else if (!a.resourcesUsed[i].distributionType.compare(
					"lognormal")) {
				NS_FATAL_ERROR(
						"combination of lognormal distributions not currently supported - to be supported soon\n");
				exit(1);
			} else {
				NS_FATAL_ERROR(
						"combination of distribution type " << a.resourcesUsed[i].distributionType << " not supported\n");
				exit(1);
			}
		}
	}

	return toReturn;
}

void ExecEnv::addPgm(Program *curPgm, Program* existPgm) {
	// Iterate through all events until we hit
	// and END statement. We delete events and
	// program branches as we go.
	int execEventIndex = 0;
	ExecutionEvent *curEventNew;
	ExecutionEvent *curEventExist;
	ExecutionEventType t;
	do {
		curEventNew = curPgm->events[execEventIndex];
		curEventExist = existPgm->events[execEventIndex];

		// If we do not have overlapping types, quit
		if (curEventNew->type != curEventExist->type) {
			NS_FATAL_ERROR(
					"Could not merge program models with types: " << curEventNew->type << " and " << curEventExist->type << std::endl);
			exit(1);
		}

		if (curEventNew->type == PROCESS) {
			// If we have a PROCESS event, we set the
			// one in the existing program to be the
			// sum of the existing and the new
			ProcessingStage curPSNew = *(ProcessingStage *) (curEventNew);
			ProcessingStage curPSExist = *(ProcessingStage *) (curEventExist);
			*curEventExist = addProcessingStages(curPSNew, curPSExist);
		} else if (curEventNew->type == CONDITION
				&& !(((Condition *) curEventNew)->condType == STATECONDITION
						&& ((StateCondition*) curEventNew)->operation
								== CONDITIONWRITE)) {
			Condition *curCond = (Condition *) (curEventNew);
			Condition *existCond = (Condition *) (curEventExist);

			// Make sure we have the same type of condition
			if (curCond->condType != existCond->condType) {
				NS_FATAL_ERROR(
						"Could not merge differing condition types: " << curCond->condType << " and " << existCond->condType << std::endl);
				exit(1);
			}

			// Here, we can safely free curPgm, as it will not
			// be used after this point. Either the conditions
			// program is inserted into the existing tree, or
			// curPgm is set to the program in the existing tree.
			delete curPgm;

			// If the value is exactly the same as one in the
			// condition, simply update the curPgm pointer to
			// the program in this entry.
			// First, get the value of the condition, which
			// resides as the only value in the condition.
			std::pair<uint32_t, Program *> newProgram =
					curCond->getClosestEntryValue(0);
			uint32_t conditionValue = newProgram.first;
			std::pair<uint32_t, Program *> closest =
					existCond->getClosestEntryValue(conditionValue);
			if (closest.first == conditionValue) {
				curPgm = newProgram.second;
				existPgm = closest.second;
				execEventIndex = 0;
				continue; // To avoid the execEventIndex++ below
			} else {
				existCond->insertEntry(conditionValue, newProgram.second);
				return;
			}
		}

		// If we do not have a process nor a condition,
		// we don't add anything, but we must make
		// sure the two events are identical. Note that we
		// have removed the packet characteristic for dequeue
		// events.
		//
		// UPDATE 210814: We remove location from the comparison,
		// because two events may correspond even if they are obtained
		// from differing locations, e.g., in the case with HIRQ-12 on
		// the N900.
		else {
			std::vector<std::string> noLocTokensNew = curEventNew->tokens;
			std::vector<std::string> noLocTokensExist = curEventExist->tokens;
			noLocTokensNew[0] = "";
			noLocTokensExist[0] = "";
			std::string curEventNewDesc = deTokenize(noLocTokensNew);
			std::string curEventExistDesc = deTokenize(noLocTokensExist);
			if (curEventNewDesc.compare(curEventExistDesc)) {
				NS_FATAL_ERROR(
						"Could not merge program models with differing event descriptors: " << std::endl << curEventNewDesc << std::endl << curEventExistDesc << std::endl);
				exit(1);

			}
		}

		// We can safely free the handled event
		t = curEventNew->type;
		delete curEventNew;
		execEventIndex++;
	} while (t != END);
}

void ExecEnv::PrintProgram(Program *curPgm) {
	//std::cout << "Listing of program " << curPgm->sem->name << ":" << std::endl;
	unsigned int i = 0;
	unsigned int numIndent = 0;

	//std::cout << curEvt->type;
	for (unsigned int j = 0; j < numIndent; j++)
		std::cout << "\t";

	while (i < curPgm->events.size()) {
		ExecutionEvent *curEvt = curPgm->events[i];
		switch (curEvt->type) {
		case LOOP: {
			std::cout << *((ExecuteExecutionEvent *) curEvt) << std::endl;
			break;
		}
		case EXECUTE: {
			std::cout << *((ExecuteExecutionEvent *) curEvt) << std::endl;
			break;
		}
		case PROCESS: {
			std::cout << *((ProcessingStage *) curEvt) << std::endl;
			break;
		}
		case SCHEDULER: {
			std::cout << *((SchedulerExecutionEvent *) curEvt) << std::endl;
			break;
		}
		case SYNCHRONIZATION: {
			std::cout << *((SynchronizationExecutionEvent *) curEvt)
					<< std::endl;
			break;
		}
		case QUEUE: {
			std::cout << *((QueueExecutionEvent *) curEvt) << std::endl;
			break;
		}
		case INTERRUPT: {
			std::cout << *((InterruptExecutionEvent *) curEvt) << std::endl;
			break;
		}
		case CONDITION: {
			std::cout << *((Condition *) curEvt) << std::endl;

			numIndent++;
			Condition *curCnd = ((Condition *) curEvt);
			std::cout << std::endl << curCnd->programs.front().first << ":"
					<< std::endl;
			curPgm = curCnd->programs.front().second;

			i = -1;
			break;
		}
		case TEMPSYNCH: {
			std::cout << *((ExecutionEvent *) curEvt) << std::endl;
			break;
		}
		case END: {
			std::cout << *((ExecutionEvent *) curEvt) << std::endl;
			break;
		}
		case DEBUG: {
			std::cout << *((ExecutionEvent *) curEvt) << std::endl;
			break;
		}
        case MEASURE: {
			std::cout << *((ExecutionEvent *) curEvt) << std::endl;
			break;
        }
		case LASTTYPE: {
			std::cout << *((ExecutionEvent *) curEvt) << std::endl;
			break;
		}
		}

		i++;
	}

	std::cout << std::endl;
	std::cout << std::endl;
}

void ExecEnv::PrintSEM(Program *curPgm, int numIndent) {
	for (int j = 0; j < numIndent; j++)
		std::cout << "\t";
	int i = 0;
	while (i < (int) curPgm->events.size()) {
		std::cout << ExecutionEvent::typeStrings[curPgm->events[i]->type]
				<< " ";
		if (curPgm->events[i]->type == CONDITION) {
			std::list<std::pair<uint32_t, Program *> >::iterator it =
					((Condition *) curPgm->events[i])->programs.begin();
			for (; it != ((Condition *) curPgm->events[i])->programs.end();
					it++) {
				std::cout << std::endl << (*it).first << ": ";
				PrintSEM((*it).second, numIndent + 1);
			}
		}
		i++;
	}

	std::cout << std::endl;
}

int lineNr = 0;
std::string line;

void ExecEnv::HandleSignature(std::vector<std::string> tokens) {
	// Temporary variables used during parsing. Since the function
	// returns for each event, while the variables hold values
	// regarding series of events, we must keep their values
	// between runs. This is why they are declared static.
	static Ptr<SEM> currentlyHandled = NULL;
	static std::vector<std::string> currentDistributions;
	static std::vector<enum ResourceType> currentResources;
	static Program *rootProgram = NULL;
	static Program *currentProgram = NULL;
	static bool dequeueOrLoopEncountered = false;
	static std::string currentName = "";
	static uint32_t nrSamples = 0;

	// Pointer to the execution event and prospective
	// condition created
	ExecutionEvent *execEvent = NULL;
	Condition *c = NULL;

	/******************************************/
	/************** SEM HEADER ****************/
	/******************************************/

	if (!tokens[0].compare("NAME")) {
		// Check if this sem allready exists. If not, create it.
		Ptr<SEM> sem;
		std::map<std::string, Ptr<SEM> >::iterator it = m_serviceMap.find(
				tokens[1]);

		// It did not exist: create it
		if (it == m_serviceMap.end() || it->second == NULL) {
			currentlyHandled = Create<SEM>();
			m_serviceMap[tokens[1]] = currentlyHandled;
			currentlyHandled->name = tokens[1];
		} 
		else // It did exist: set currentlyHandled to it
			currentlyHandled = m_serviceMap[tokens[1]];

		// If we have a trigger specified on this service, we
		// insert the string into the sem
		std::map<std::string, std::string>::iterator foundTrigger =
				serviceTriggers.find(tokens[1]);

		if (foundTrigger != serviceTriggers.end()) {
			currentlyHandled->trigger = foundTrigger->second;
			serviceTriggerMap[foundTrigger->second] = currentlyHandled;
		}

		// Set the name, used by e.g., LOOPSTART
		currentName = tokens[1];
	}

	// Just set the PEU in the currently handled SEM
	if (!tokens[0].compare("PEU")) {
		if (is_prefix("cpu", tokens[1])) {

            if (!tokens[1].compare("cpu1")) {
                currentlyHandled->peu = hwModel->cpus[1];
            } else {
                currentlyHandled->peu = hwModel->cpus[0];
            }

		} else
			currentlyHandled->peu = hwModel->m_PEUs[tokens[1]];
	}

	if (!tokens[0].compare("RESOURCES")) {
		if (!tokens[1].compare("cycles"))
			currentResources.push_back(CYCLES);
		else if (!tokens[1].compare("nanoseconds"))
			currentResources.push_back(NANOSECONDS);
		else if (!tokens[1].compare("instructions"))
			currentResources.push_back(INSTRUCTIONS);
		else if (!tokens[1].compare("cachemisses"))
			currentResources.push_back(CACHEMISSES);
		else if (!tokens[1].compare("memoryaccesses"))
			currentResources.push_back(MEMORYACCESSES);
		else if (!tokens[1].compare("memorystallcycles"))
			currentResources.push_back(MEMSTALLCYCLES);

		// Easier if we just add the string here, and
		// act according to this when handling a PROCESS
		// event below
		currentDistributions.push_back(tokens[2]);
	}

	if (!tokens[0].compare("FRACTION")) {
		// Set samples
		nrSamples = stringToUint32(tokens[2]);
	}

	/**************************************/
	/************** EVENTS ****************/
	/**************************************/

	// When we encounter START, instantiate
	// a new root condition
	if (!tokens[1].compare("START") || !tokens[1].compare("LOOPSTART")) {
		currentProgram = new Program();
		rootProgram = currentProgram;

		// CurrentlyHandled is set when encountering the
		// NAME field in the signature header
		currentProgram->sem = currentlyHandled;

		// If we have a LOOPSTART, set the queues
		// served and any additional conditions in
		// the loopcondition.
		if (!tokens[1].compare("LOOPSTART") && !currentlyHandled->lc) {
			currentlyHandled->lc = new LoopCondition;

			// Fill queues only if the loop is based on that
			if (tokens[3].compare("noloc")) {
				currentlyHandled->lc->serviceQueues = !(serviceQueues.find(
						tokens[3]) == serviceQueues.end());
				currentlyHandled->lc->stateQueues = !(std::find(
						stateQueueOrder.begin(), stateQueueOrder.end(),
						tokens[3]) == stateQueueOrder.end());
				fillQueues(tokens[3], tokens[4], currentlyHandled->lc);
			}

			// Check if we have specified an additional
			// condition for this queue
			std::map<std::string, struct condition>::iterator foundLoopCond =
					loopConditions.find(currentName);
			if (foundLoopCond != loopConditions.end()) {
				currentlyHandled->lc->additionalCondition =
						conditionFunctions->conditionMap[foundLoopCond->second.condName];
				currentlyHandled->lc->hasAdditionalCondition = true;
			}
		}
	}

	if (!tokens[1].compare("STOP") || !tokens[1].compare("RESTART")) {
		// Append END-event to current program
		ExecutionEvent *end = new ExecutionEvent();
		end->type = END;
		currentProgram->events.push_back(end);
		execEvent = end;
		execEvent->line = line;
		execEvent->tokens = tokens;

		// Print the constructed program for debug purposes
//			Program *curPgm = rootProgram;
//		PrintProgram(curPgm);

		// Get the program pointer of the current SEM
		Program **existingProgram;
		if (currentlyHandled->lc != NULL) {
			uint32_t numQueues =
					currentlyHandled->lc->serviceQueues ?
							currentlyHandled->lc->serviceQueuesServed.size() :
							(currentlyHandled->lc->stateQueues ?
									currentlyHandled->lc->stateQueuesServed.size() :
									currentlyHandled->lc->queuesServed.size());

			if (numQueues > 0) {
				if (dequeueOrLoopEncountered)
					existingProgram = &(currentlyHandled->rootProgram);
				else
					existingProgram = &(currentlyHandled->lc->emptyQueues);
			} else
				existingProgram = &(currentlyHandled->rootProgram);
		} else
			existingProgram = &(currentlyHandled->rootProgram);


		// Since we may delete this event in addPgm (below), we must make sure
		// to set any prospective trigger and debug data on this event
		// before returning. For other event types, this is done at the end
		// of this member function.
		execEvent->tokens = tokens;
		std::map<std::string, std::string>::iterator foundTrigger =
				locationTriggers.find(tokens[0]);
		if (foundTrigger != locationTriggers.end())
			execEvent->checkpoint = locationTriggers[foundTrigger->first];
		execEvent->line = line;
		execEvent->lineNr = lineNr;
		if (tokens.size() > 1 && !tokens[tokens.size() - 2].compare("debug")) {
			execEvent->hasDebug = true;
			execEvent->debug = tokens[tokens.size() - 1];
		} else
			execEvent->hasDebug = false;

		// If the existing program is NULL, we simply set it
		// to our program. Else wise, we iterate the existing
		// program to update probabilities and merge branch
		if (*existingProgram == NULL)
			*existingProgram = rootProgram;
		else
			// addPgm will delete the parts of
			// curPgm not inserted into the tree
			addPgm(rootProgram, *existingProgram);

		// Reset static var
		currentlyHandled = NULL;
		currentDistributions.erase(currentDistributions.begin(),
				currentDistributions.end());
		currentResources.erase(currentResources.begin(),
				currentResources.end());
		rootProgram = NULL;
		currentProgram = NULL;
		dequeueOrLoopEncountered = false;
		currentName = "";
		nrSamples = 0;

		return;
	}

	if (!tokens[1].compare("PROCESS") || !tokens[1].compare("PEUSTART")) {
		// Iterate all HWE aggregates obtained during
		// the parsing of the header.
		// First, create the processing stage
		ProcessingStage *ps = new ProcessingStage();
		execEvent = ps;
		ps->samples = nrSamples;

		int intField = 0;
		if (!tokens[1].compare("PEUSTART"))
			intField = 1;

		// Then, iterate according to all HWE aggregates
		// specified in the header parsed above.
		int numHWEs = currentResources.size();
		int tokenIndex = 0;
		for (int i = 0; i < numHWEs; i++) {
			// Obtain the parameters of the given distribution
			if (!currentDistributions[i].compare("normal")) {
				// The normal distribution takes two parameters:
				// average and standard deviation
				double average = stringToDouble(
						tokens[intField + 2 + tokenIndex++]);
				double sd = stringToDouble(tokens[intField + 2 + tokenIndex++]);
				ps->resourcesUsed[currentResources[i]].defined = true;
				ps->resourcesUsed[currentResources[i]].consumption =
						NormalVariable(average, sd * sd);
				ps->resourcesUsed[currentResources[i]].distributionType =
						"normal";
				ps->resourcesUsed[currentResources[i]].param1 = average;
				ps->resourcesUsed[currentResources[i]].param2 = sd * sd;
				ps->samples = nrSamples;
			} else if (!currentDistributions[i].compare("lognormal")) {
				// The normal distribution takes two parameters:
				// average and standard deviation
				double logaverage = stringToDouble(
						tokens[intField + 2 + tokenIndex++]);
				double logsd = stringToDouble(
						tokens[intField + 2 + tokenIndex++]);
				ps->resourcesUsed[currentResources[i]].defined = true;
				ps->resourcesUsed[currentResources[i]].consumption =
						LogNormalVariable(logaverage, logsd);
				ps->resourcesUsed[currentResources[i]].distributionType =
						"lognormal";
				ps->resourcesUsed[currentResources[i]].param1 = logaverage;
				ps->resourcesUsed[currentResources[i]].param2 = logsd;
				ps->samples = nrSamples;
			}

			//
			// TODO: other distributions - we currently only support normal and lognormal
			// distributions. Lognormal appears to be the better estimator for cycles.
			//
		}

		// If we have a PEUSTART, we want insert the SEM of the interrupt specified
		if (!tokens[1].compare("PEUSTART")) {
			ps->interrupt = m_serviceMap[tokens[2]];
			if (ps->interrupt == NULL) {
				std::cout << "SEM " << tokens[2] << " is not defined. Make sure it is defined above the function that calls invokes it" << std::endl;
				exit(1);
			}
		}

		QueueExecutionEvent *q;
		if (tokens[1] == "PROCESS" && tokens.size() >= 5 && tokens[4] == "PERBYTE") {  // Espen
		    ps->pktqueue = queues[tokens[5]];  // The specified packet queue will be dequeued from.
		} else if (tokens[1] == "PEUSTART" && tokens.size() >= 6 && tokens[5] == "PERBYTE") {
		    ps->pktqueue = queues[tokens[6]];  // The specified packet queue will be dequeued from.
		}

		// Add this PS to the current program
		currentProgram->events.push_back(ps);
	}

	// Remember: unless the queue is explicitly specified,
	// which happens only outside of loops, we don't have
	// to specify any queue inside of this event. This is
	// because then, which queue to serve is determined by
	// the encapsulating loop structure.
	// Note: We have to add queue events before conditions,
	// because if there is a condition on a dequeued packet,
	// it must be set to curPkt in thread.cc before being
	// able to resolve the condition.
	if (!tokens[1].compare("ENQUEUE") || !tokens[1].compare("DEQUEUE")) {
		// Create an event, and insert the queue
		QueueExecutionEvent *q = new QueueExecutionEvent();
		execEvent = q;
		q->enqueue = !tokens[1].compare("ENQUEUE");
		// If we have a service queue, set the SEM
		if (!tokens[2].compare("SRVQUEUE")) {
			q->serviceQueue = true;
			if (!tokens[1].compare("ENQUEUE")) {
				q->semToEnqueue = m_serviceMap[tokens[3]];
				if (q->semToEnqueue == NULL) {
					std::cout << "SEM " << tokens[2] << " is not defined. Make sure it is defined above the function that calls invokes it" << std::endl;
					exit(1);
				}
			}
		} else if (!tokens[2].compare("STATEQUEUE")) {
			q->stateQueue = true;
			if (!tokens[1].compare("ENQUEUE")) {
				q->valueToEnqueue = stringToUint32(tokens[3]);
			}

			// We currently only support local statequeues, but might
			// later, on a need-to-implement basis, extend this support
			// also to packet and service queues.
			//
			// We also do not support global state queues for now, as
			// this is not in demand (i.e., we're only using state
			// queues for the spisizes queue on the N900).
			q->local = !tokens[5].compare("local");
		}

		// We specify the queue
		if (tokens[4].compare("0")) {
			if (q->stateQueue)
				q->queueName = tokens[4]; // Only local scope supported for now
			else if (!q->serviceQueue) {
				q->queue = queues[tokens[4]];
			} else // When specifying queue in a SRVQUEUE event
				q->servQueue = serviceQueues[tokens[4]];
		}

		// If we have a checkpoint specified for the queue,
		// set it. NOTICE that we may fire TWO triggers upon
		// this event: (1) the one in the DEQUEUE event itself,
		// and (2) the one in the SEM prospectively dequeued.
		if (!tokens[1].compare("DEQUEUE")) {
			std::map<std::string, std::string>::iterator dqTrigIt =
					dequeueTriggers.find(tokens[3]);
			if (dqTrigIt != dequeueTriggers.end())
				q->checkpoint = dqTrigIt->second;

			// If we're inside a loop, update
			// dequeueOrLoopEncountered
			if (currentlyHandled->lc != NULL
					&& (!tokens[4].compare("0")
							|| queuesIn(tokens[4], tokens[4],
									currentlyHandled->lc))) {
				dequeueOrLoopEncountered = true;
				currentProgram->hasDequeue = true; // See comments in program.h this member
			}
		}

		// Add the event to the current program
		currentProgram->events.push_back(q);


        if (q->serviceQueue == true && !tokens[1].compare("ENQUEUE") && tokens.size() > 5) {
		    std::vector<uint32_t> arguments;
		    std::string threadName = tokens[5];

		    SchedulerExecutionEvent *se = new SchedulerExecutionEvent(
				    AWAKE, arguments,
				    threadName);

		    //execEvent = se;

            currentProgram->events.push_back(se);
		}
	}

	// Handle conditions
	if (!tokens[1].compare("QUEUECOND") || !tokens[1].compare("THREADCOND")
			|| !tokens[1].compare("STATECOND") || !tokens[1].compare("ENQUEUE")
			|| !tokens[1].compare("DEQUEUE") || !tokens[1].compare("PKTEXTR")) {

		Program *newProgram = new Program();
		newProgram->sem = currentlyHandled;

		// All conditions are assumed to be local.
		// State-conditions can be used to create
		// global conditions.
		if (!tokens[1].compare("QUEUECOND")) {
			// Assume that the first and last queues are of the same type: packet or service queue
			if (queues.find(tokens[2]) != queues.end()) {
				QueueCondition *q = new QueueCondition();
				((ExecutionEvent *)q)-> lineNr = lineNr;
				q->firstQueue = queues[tokens[2]];
				q->lastQueue = queues[tokens[3]];
				c = (Condition *) q;
				c->scope = CONDITIONGLOBAL;
				c->insertEntry(
						!tokens[4].compare("empty") ?
								QUEUEEMPTY : QUEUENOTEMPTY, newProgram);
				c->getConditionQueues = ns3::MakeCallback(
						&ConditionFunctions::QueueCondition,
						conditionFunctions);
			} else {
				ServiceQueueCondition *q = new ServiceQueueCondition();
				q->firstQueue = serviceQueues[tokens[2]];
				q->lastQueue = serviceQueues[tokens[3]];
				((ExecutionEvent *)q)-> lineNr = lineNr;
				c = (Condition *) q;
				c->insertEntry(
						!tokens[4].compare("empty") ?
								QUEUEEMPTY : QUEUENOTEMPTY, newProgram);
				c->getServiceConditionQueues = ns3::MakeCallback(
						&ConditionFunctions::ServiceQueueCondition,
						conditionFunctions);
			}

//			std::cout << "Adding CONDITION" << std::endl;
			// Assume local: insert c into current program
			currentProgram->events.push_back(c);
			currentProgram = newProgram;
		} else if (!tokens[1].compare("THREADCOND")) {
//			std::cout << "Adding CONDITION" << std::endl;
			ThreadCondition *t = new ThreadCondition();
			((ExecutionEvent *)t)-> lineNr = lineNr;
			c = (Condition *) t;
			c->scope = CONDITIONGLOBAL;
			c->insertEntry(
					!tokens[4].compare("ready") ? THREADREADY : THREADNOTREADY,
					newProgram);
			t->threadId = tokens[2];
			c->getConditionThread = ns3::MakeCallback(
					&ConditionFunctions::ThreadCondition, conditionFunctions);

			// Assume local: insert c into current program
			currentProgram->events.push_back(c);
			currentProgram = newProgram;
		} else if (!tokens[1].compare("STATECOND")) {
//			std::cout << "Adding rCONDITION" << std::endl;
			// Can be global or local
			// First find out if we have a condition specified for this location
			bool definedInDeviceFile = !tokens[2].compare(
					"definedindevicefile");
			std::map<std::string, struct condition>::iterator foundCond;

			if (definedInDeviceFile)
				std::map<std::string, struct condition>::iterator foundCond =
						locationConditions.find(tokens[0]);

			if (!definedInDeviceFile || foundCond != locationConditions.end()) {
				c = (Condition *) new StateCondition();
				((ExecutionEvent *)c)-> lineNr = lineNr;
				StateCondition* sc = (StateCondition *) c;
				sc->name =
						definedInDeviceFile ?
								foundCond->second.condName : tokens[2];
				sc->operation =
						!tokens[3].compare("write") ?
								CONDITIONWRITE : CONDITIONREAD;
				sc->scope =
						!tokens[4].compare("local") ?
								CONDITIONLOCAL : CONDITIONGLOBAL;

				// CONT HERE - TODO: add name of variable if local, add to gobal vars.
				// structure if not local. We currently assume that the values are
				// integers.

				// See if state condition has read and/or write functions (TODO: Not tested yet!)
				std::map<std::string,
						Callback<uint32_t,
								Ptr<Thread> > >::iterator foundCond =
						conditionFunctions->conditionMap.find(
								"readState" + sc->name);
				if (foundCond != conditionFunctions->conditionMap.end()) {
					sc->getConditionState =
							conditionFunctions->conditionMap["readState"
									+ sc->name];
					sc->hasGetterFunction = true;
				}

				std::map<std::string,
						Callback<void,
								Ptr<Thread>, uint32_t> >::iterator foundCondWrite =
						conditionFunctions->writeConditionMap.find(
								"writeState" + sc->name);
				if (foundCondWrite
						!= conditionFunctions->writeConditionMap.end()) {
					sc->setConditionState =
							conditionFunctions->writeConditionMap["writeState"
									+ sc->name];
					sc->hasSetterFunction = true;
				}

				// Store the condition in the current program
				currentProgram->events.push_back(c);

				// Insert new program if condition is a read-condition.
				if (sc->operation == CONDITIONREAD) {
					// Insert new program into condition and make newProgram the currentProgram
					c->insertEntry(stringToUint32(tokens[5]), newProgram);
					currentProgram = newProgram;
				} else if (sc->operation == CONDITIONWRITE) {
					sc->value = stringToUint32(tokens[5]);
				}

			}
		} else if (!tokens[1].compare("PKTEXTR")) {
//			std::cout << "Adding CONDITION" << std::endl;
			// Can be global or local
			// First find out if we have a condition specified for this location
			std::map<std::string, struct condition>::iterator foundCond =
					locationConditions.find(tokens[0]);
			if (foundCond != locationConditions.end()) {
				c = (Condition *) new PacketCharacteristic();
				((ExecutionEvent *)c)-> lineNr = lineNr;

				// Set the packet extraction function id, and insert new program
				c->getConditionState =
						conditionFunctions->conditionMap[foundCond->second.condName];
				c->insertEntry(stringToUint32(tokens[2]), newProgram);

				// Make newProgram the currentProgram after pushing this condition
				currentProgram->events.push_back(c);
				currentProgram = newProgram;
			}
		} else if (!tokens[1].compare("DEQUEUE") ||
//				!tokens[1].compare("DEQUEUEN") ||
				!tokens[1].compare("ENQUEUE")) {			// ||
//				!tokens[1].compare("ENQUEUEN")) {
						// If queue is 0, we must first find the queue name.
						// But before that, we must confirm that we are in
						// fact within a loop.
			std::string queueName;
			if (!tokens[4].compare("0")) {
				if (currentlyHandled->lc == NULL) {
					NS_FATAL_ERROR("Got queue 0 outside of loop");
					exit(1);

					// Elsewise, assume that all queues in the loop
					// use the same extractor.
				} else {
					// Differentiate between service and packet queues
					if (currentlyHandled->lc->serviceQueues) {
						std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *firstQueue =
								currentlyHandled->lc->serviceQueuesServed[0];

						// First, find the queue name
						std::map<
								std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *,
								std::string>::iterator name =
								serviceQueueNames.find(firstQueue);
						if (name != serviceQueueNames.end()) {
							queueName = (*name).second;
						}
					} else if (currentlyHandled->lc->stateQueues) {
						// First, find the queue name
						Ptr<StateVariableQueue> firstQueue =
								currentlyHandled->lc->stateQueuesServed[0];

						// First, find the queue name
						std::map<Ptr<StateVariableQueue>, std::string>::iterator name =
								stateQueueNames.find(firstQueue);
						if (name != stateQueueNames.end()) {
							queueName = (*name).second;
						}
					} else {
						Ptr<Queue> firstQueue =
								currentlyHandled->lc->queuesServed[0];

						// First, find the queue name
						std::map<Ptr<Queue>, std::string>::iterator name =
								queueNames.find(firstQueue);
						if (name != queueNames.end()) {
							queueName = (*name).second;
						}
					}
				}
			}

			// Elsewise, we have explicitly specified the queue name. This is
			// quite uncommon, as this means the analyser have not removed the
			// name, and we are de-queuing outside of a loop.
			else
				queueName = tokens[4];

			// Now check if there is a condition on it.
			std::map<std::string, std::vector<struct condition> >::iterator condition =
					!tokens[1].compare(
							"DEQUEUE")/* || !tokens[3].compare("DEQUEUEN")*/?
							dequeueConditions.find(queueName) :
							enqueueConditions.find(queueName);

			// If so, add the condition.
			if (!(condition == dequeueConditions.end()
					|| condition == enqueueConditions.end())) {

				// Iterate all conditions on this queue, and chain together
				std::vector<struct condition> allConditions = condition->second;
				struct condition cond;
				for (std::vector<struct condition>::iterator it =
						allConditions.begin(); it != allConditions.end();
						++it) {
					newProgram = new Program();
					newProgram->sem = currentlyHandled;

					cond = *it;
					PacketCharacteristic *pc = new PacketCharacteristic();
					c = (Condition *) pc;
					((ExecutionEvent *)pc)-> lineNr = lineNr;
					c->scope = CONDITIONGLOBAL;

					// Set the packet extraction function id, and insert new program
					c->getConditionState =
							conditionFunctions->conditionMap[cond.condName];
					c->insertEntry(stringToUint32(tokens[3]), newProgram);

					// Make newProgram the currentProgram after pushing this condition
					currentProgram->events.push_back(c);
					currentProgram = newProgram;
				}
			}

			if (!tokens[2].compare("STATEQUEUE")
					&& !tokens[1].compare("DEQUEUE")) {
				// CONT HERE: move to thread.cc and handle the execution-part
				newProgram = new Program();
				newProgram->sem = currentlyHandled;

				StateQueueCondition *sc = new StateQueueCondition();
				((ExecutionEvent *)sc)-> lineNr = lineNr;
				sc->queueName = queueName;
				c = (Condition *) sc;

				// Set the packet extraction function id, and insert new program
				c->insertEntry(stringToUint32(tokens[3]), newProgram);

				// Make newProgram the currentProgram after pushing this condition
				currentProgram->events.push_back(c);
				currentProgram = newProgram;
			}
		}
	}

	if (!tokens[1].compare("CALL") || !tokens[1].compare("LOOP")) {
		// Here, we have one service calling another. We create
		// an ExecutionEvent, where the service is inserted
		// into the "service" member.
		ExecuteExecutionEvent *e = new ExecuteExecutionEvent();
		e->service = !tokens[2].compare("0") ? "" : tokens[2];
		if (!tokens[2].compare("0"))
			e->sem = NULL;
		else {
			e->sem = m_serviceMap[tokens[2]];
			// Device file is set up incorrectly
			if (e->sem == NULL) {
				std::cout << "SEM " << tokens[2] << " is not defined. Make sure it is defined above the function that calls invokes it" << std::endl;
				exit(1);
			}
		}
		execEvent = e;

		// If we have a call to a loop, prepare a loop condition,
		// and set dequeueOrLoopEncountered to true if it regards
		// queue(s) of a prospectively encapsulating loop.
		if (!tokens[1].compare("LOOP")) {
			// We first copy the contents of the lc of the target
			// service if it has one. Note that this includes the
			// queues served, which may imply quite a bit of copying,
			// but its bettwe to do it once here during initialization
			// than repeatedly per execution during simulation.
			e->lc = new LoopCondition();
			LoopCondition *targetServiceLC = this->m_serviceMap[tokens[2]]->lc;
			if (targetServiceLC != NULL)
				*(e->lc) = *targetServiceLC;

			// It is reasonable that parameters of the loop may
			// change. This does not regard which queues served,
			// though, as this is part of what defined the loop.
			e->lc->maxIterations = stringToUint32(tokens[6]);
			e->lc->perQueue = !tokens[3].compare("1");

			// If we have a nested loop then
			// if we have a loop that servers one or more of the
			// queues in currentlyHandled->lc, set
			// dequeueOrLoopEncountered to true.
			if (currentlyHandled->lc != NULL
					&& queuesIn(tokens[3], tokens[4], currentlyHandled->lc)) {
				dequeueOrLoopEncountered = true;
				currentProgram->hasInternalLoop = true; // Se comments in program.h on this member
			}
		}

		// Insert the event into the current program
		currentProgram->events.push_back(e);
	}

	if (!tokens[1].compare("SEMUP") || !tokens[1].compare("SEMDOWN")
			|| !tokens[1].compare("WAITCOMPL") || !tokens[1].compare("COMPL")) {
		// Note that this may be on a tempsynch
		// This is currently how it works in Linux,
		// but it could easily be changed to be
		// OS/scheduler-independent as the co-sim
		// mechanism impl. by TaskScheduler
		// provisions for that with the synchType etc.
		// Ideally, the trace function should take
		// the synch type as an argument. TODO if time.
		// Alternatively, just let the analyzer set
		// the number according to the type of the
		// event.
		uint32_t synchType =
				currentlyHandled->peu->taskScheduler->GetSynchReqType(
						tokens[1]);
		SynchronizationExecutionEvent *s = new SynchronizationExecutionEvent();
		execEvent = s;
		s->synchType = synchType;

	NS_ASSERT_MSG(tokens.size() >= 4, "Expected at least 4 tokens in synch request on line " << lineNr);

		s->id = tokens[2];
		s->temp = !tokens[2].compare("(TEMP)");
		s->global = !tokens[3].compare("global"); // TODO: scope currently only supported for tempvars
		// Currently, we don't use args, so we don't touch it.

		// Add the event to the current program
		currentProgram->events.push_back(s);
	}

	if (!tokens[1].compare("TEMPSYNCH")) {
		// This is very simple - we just indicate its type.
		// All of the action (creation of synch-prim. and
		// setting the void-pointer in the packet to it <- NOT PACKET, IT MAY NOT EXIST!)
		// happens in thread.cc
		TempCompletion *tc = new TempCompletion();
		execEvent = tc;

		/* Go through list of users, and add to vector in the tempsynch statement */
		/* We know that these come in pairs: (id number, service id) */
		unsigned int i = 3;
		for (; i < tokens.size(); i += 2)
			tc->users.push_back(tokens[i]);

		tc->global = !tokens[tokens.size() - 1].compare("global");

		currentProgram->events.push_back(tc);
	}

	if (!tokens[1].compare("DEBUG")) {
		// This is very simple - we just indicate its type.
		// All of the action (creation of synch-prim. and
		// setting the void-pointer in the packet to it <- NOT PACKET, IT MAY NOT EXIST!)
		// happens in thread.cc
		DebugExecutionEvent *de = new DebugExecutionEvent();
		execEvent = de;
		if (tokens.size() >= 2)
			de->tag = tokens[2];

		currentProgram->events.push_back(de);
	}

    if (!tokens[1].compare("MEASURE")) {
        MeasureExecutionEvent *me = new MeasureExecutionEvent();
        currentProgram->events.push_back(me);
    }

	if (!tokens[1].compare("TTWAKEUP") || !tokens[1].compare("SLEEP")) {
		std::vector<uint32_t> arguments;
		std::string threadName = ""; // Used only if TTWAKEUP
		if (!tokens[1].compare("TTWAKEUP"))
			threadName = tokens[2];

		SchedulerExecutionEvent *se = new SchedulerExecutionEvent(
				!tokens[1].compare("TTWAKEUP") ? AWAKE : SLEEP, arguments,
				threadName);

		execEvent = se;

		currentProgram->events.push_back(se);
	}

	if (execEvent) {
		// We must set the packet characteristic to "0" for queue events, else wise
		// addPgm will complain when encountering two events differing in
		// terms of the packet extract. Differing packet extracts does
		// not mean that the events differ, only that the packets dequeued
		// have diferent characteristics.
		if (!tokens[1].compare("DEQUEUE") ||
//				!tokens[1].compare("DEQUEUEN") ||
				!tokens[1].compare("ENQUEUE"))		// ||
//				!tokens[1].compare("ENQUEUEN"))
			tokens[3] = "0";

		execEvent->tokens = tokens;

		// If we have a checkpoint specified on this location, we
		// insert the string into the checkpoint value
		std::map<std::string, std::string>::iterator foundTrigger =
				locationTriggers.find(tokens[0]);

		if (foundTrigger != locationTriggers.end())
			execEvent->checkpoint = locationTriggers[foundTrigger->first];

		// DEBUG
		execEvent->line = line;
		execEvent->lineNr = lineNr;
//		std::cout << tokens.size() << " " << tokens[tokens.size() - 2] << std::endl;
		if (tokens.size() > 1 && !tokens[tokens.size() - 2].compare("debug")) {
			execEvent->hasDebug = true;
			execEvent->debug = tokens[tokens.size() - 1];
		} else
			execEvent->hasDebug = false;
		// !DEBUG
	}

	if (c) {
		c->tokens = tokens;
		c->lineNr = lineNr;
		c->line = line;

		// If we have a checkpoint specified on this location, we
		// insert the string into the checkpoint value
		std::map<std::string, std::string>::iterator foundTrigger =
				locationTriggers.find(tokens[0]);

		if (foundTrigger != locationTriggers.end())
			c->checkpoint = locationTriggers[foundTrigger->first];

//		std::cout << tokens.size() << " " << tokens[tokens.size() - 2] << std::endl;
		if (tokens.size() > 1 && !tokens[tokens.size() - 2].compare("debug")) {
			c->hasDebug = true;
			c->debug = tokens[tokens.size() - 1];
		}
	}
}

std::vector<std::string> split(const char *str, char c = ' ')
{
    std::vector<std::string> result;

    do
    {
        const char *begin = str;

        while(*str != c && *str)
            str++;

        result.push_back(std::string(begin, str));
    } while (0 != *str++);

    return result;
}

// Parse device file to create
// - task scheulder
// - SEMs
// - LEUs
// - PEUs
// - Queues
// - Snchronization primitives
void ExecEnv::Parse(std::string device) {
	std::ifstream myfile;
	std::string mode;
	myfile.open(device.c_str());
	if (myfile.is_open()) {
		// Obtain service mapping
		while (myfile.good()) {
			// Get line and tokenize
			lineNr++;
//			if(lineNr == 1227) {
//				std::cout << "here" << std::endl;
//			}
			std::getline(myfile, line);

//			std::cout << "INTERPRETING LINE: " << lineNr << ": " << line << std::endl;

			std::istringstream iss(line);
			std::vector<std::string> tokens; // = split(line.c_str());
			std::copy(std::istream_iterator<std::string>(iss),
					std::istream_iterator<std::string>(),
					std::back_inserter<std::vector<std::string> >(tokens));

			// If there was a blank line, we know we
			// are done
			if (tokens.size() == 0 || tokens[0].c_str()[0] == '#')
				continue;

			// Mode changes with these keywords
			if (!tokens[0].compare("QUEUES") || !tokens[0].compare("SYNCH")
					|| !tokens[0].compare("THREADS")
					|| !tokens[0].compare("SIGSTART")
					|| !tokens[0].compare("SIGEND")
					|| !tokens[0].compare("HARDWARE")
					|| !tokens[0].compare("CONDITIONS")
					|| !tokens[0].compare("TRIGGERS")) {

				mode = tokens[0];
				continue;
			}

			// Parse the device header
			if (!mode.compare("QUEUES")) {
				HandleQueue(tokens);
			} else if (!mode.compare("SYNCH")) {
				if (hwModel->cpus[0]->taskScheduler == NULL) {
					NS_FATAL_ERROR(
							"Attempting to create synchronization variable without having set the task scheduler\n");
					exit(1);
				}

				HandleSynch(tokens);
			} else if (!mode.compare("THREADS")) {
				HandleThreads(tokens);
			} else if (!mode.compare("HARDWARE")) {
				HandleHardware(tokens);
			} else if (!mode.compare("CONDITIONS")) {
				HandleConditions(tokens);
			} else if (!mode.compare("TRIGGERS")) {
				HandleTriggers(tokens);
			} else if (!mode.compare("SIGSTART")) {
				HandleSignature(tokens);
			} else
				continue;

			// Act according to the mode:
			// queue: add to queue with correct parametesr
			// sych: use taskscheduler of cpu to add synch
			// threads: add to local map thread<->program for later forkin when all sigs (e.i., the whole file) is parsed
			// sigstart: just set the mode
			// scheduler: create the taskscheduler of the cpu looking up the class name in the scheduler<->schedsim static
			//      map in TaskScheduler (e.g., "Linux" <-> "LinSched", and using an objectfactory to create an object of that type.
			//      We support other types of schedulers for other PEUs as well, but if no scheduler is specified, a simple ParallellThreadsScheduler
			//      is instantiated for that PEU.
			// condition: register the condition in the local addr<->conditionname map for later addition to execution events during event parsing

			// Parse events to create SEMs (note, only later on some of the SEMs are used as roots at threads and PEUs!):
			// name and distribution: store in local string
			// resources: add to local vector of strings
			//
			// SIGEND: break

		}
	} else
		std::cout << "Unable to open file" << std::endl;

	myfile.close();

	// Print SEMs
	/*std::map<std::string, Ptr<SEM> >::iterator it;
	for (it = m_serviceMap.begin(); it != m_serviceMap.end(); it++) {
		std::cout << it->second->name << ": " << std::endl;
		if (it->second->rootProgram)
			PrintSEM(it->second->rootProgram, 1);
	}*/
}

}
