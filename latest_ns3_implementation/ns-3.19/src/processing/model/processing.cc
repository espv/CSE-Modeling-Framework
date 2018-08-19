#include "ns3/object.h"
#include "processing.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

#include "hwmodel.h"
#include "program.h"
#include "peu.h"
#include "membus.h"
#include "interrupt-controller.h"
#include <ns3/drop-tail-queue.h>

#include <iostream>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <stdexcept>

#include "hwmodel.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Processing");
NS_OBJECT_ENSURE_REGISTERED (Processing);

class BadConversion : public std::runtime_error {
public:
  BadConversion(std::string const& s)
    : std::runtime_error(s)
  { }
};

TypeId
Processing::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Processing")
    .SetParent<Object> ()
    .AddConstructor<Processing> ()
  ;
  return tid;
}
  
Processing::Processing()
{
}

// DEPRECATED
// double
// Processing::GetDuration(double cpucycles) {
//   // cycles / Mhz = seconds * 1000000
//   // nanoseconds = (seconds * 1000000) * 1000
//   return cpucycles / m_cpufreq * 1000;
// }

void
Processing::RegisterSEM(std::string serviceName, Ptr<SEM> s)
{
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end())
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();

  spi = m_serviceMap[serviceName];
  spi->sem = s;

  std::cout << "Registered SEM: " << serviceName << std::endl;
}

void
Processing::Initialize(std::string device)
{
  // Create hardware model
  Ptr<HWModel> hwMod = CreateObject<HWModel> ();
  hwModel = hwMod;

  // Create conditions object
  conditionFunctions = CreateObject<ConditionFunctions> ();
  conditionFunctions->Initialize(GetObject<Processing> ());

  // Create interrupt controller for the hwModel
  Ptr<InterruptController> ic = CreateObject<InterruptController> ();
  hwModel->m_interruptController = ic;
  hwModel->processing = GetObject<Processing> ();
  
  // Parse device file to create the rest
  Parse(device);
}

void Processing::HandleQueue(std::vector<std::string> tokens) {
  // Go throught types (currently only support FIFO)
  if(!tokens[1].compare("FIFO")) {

    // No size = no upper bound on contents
    if(!tokens[2].compare("-1"))
      queues[tokens[0]] = CreateObjectWithAttributes<DropTailQueue> ("MaxPackets", UintegerValue(4294967295));

    // We have a size
    else {
      // Get size
      std::istringstream i(tokens[2]);
      int size;
      if (!(i >> size))
	NS_FATAL_ERROR("Unable to convert queue size " << tokens[2] << " to integer" << std::endl);
      // Act according to units
      if(!tokens[3].compare("packets")) {
	queues[tokens[0]] = CreateObjectWithAttributes<DropTailQueue> ("MaxPackets", UintegerValue(size));
	std::cout << "Added FIFO queue " << tokens[0] << " with size " << size << " packet" << std::endl;
      }
      else {
	queues[tokens[0]] = CreateObjectWithAttributes<DropTailQueue> ("MaxBytes", UintegerValue(size));	
	std::cout << "Added FIFO queue " << tokens[0] << " with size " << size << " bytes" << std::endl;
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

void Processing::HandleSynch(std::vector<std::string> tokens) {
  // Get the name
  std::istringstream i;
  i.str(tokens[0]);
  std::string name;
  if (!(i >> name)) NS_FATAL_ERROR("Unable to parse synch name " << tokens[0] << std::endl);
  
  // Get the type
  std::istringstream i2;
  i2.str(tokens[1]);
  unsigned int type;
  if (!(i2 >> type)) NS_FATAL_ERROR("Unable to parse synch type " << tokens[1]  << std::endl);

  std::cout << "Adding synch " << name << " of type " << type << " with args ";
  // get arguments by iteration the rest of the line
  unsigned int j = 2;
  uint32_t argvalue;
  std::vector<uint32_t> arguments;
  for(; j < tokens.size(); j++) {
    i.str(tokens[j]);
    if (!(i >> argvalue))
      NS_FATAL_ERROR("Unable to convert synch parse " << tokens[j] << " to integer" << std::endl);

    arguments.push_back(argvalue);

    std::cout << argvalue << " ";
  }

  std::cout << std::endl;

  // Allocate the synch primitive
  hwModel->cpu->taskScheduler->AllocateSynch(type, name, arguments);
}

void Processing::HandleThreads(std::vector<std::string> tokens) {
}

void Processing::HandleHardware(std::vector<std::string> tokens) {
  // get type, name, frequency (Mhz) and scheduler (if peu)
  std::istringstream i;

  // Parse frequency
  i.str(tokens[1]); int freq; if(!(i >> freq)) {NS_FATAL_ERROR("Unable to parse frequency " << tokens[1] << std::endl); exit(1);}

  // Act accordint to type
  // If we have a membus, create and install in hardware
  if(!tokens[0].compare("MEMBUS")) {
    // Create memory bus for the hwModel
    Ptr<MemBus> membus = CreateObjectWithAttributes<MemBus> ("frequency", UintegerValue(freq));
    hwModel->m_memBus = membus;
  }

  // If we have a PEU, create and install.
  else if (!tokens[0].compare("PEU")) {
    Ptr<PEU> newPEU;

    // Treat the name CPU specially. As it is the only PEU that
    // can be intteruted, is has its own type and member variable in hwModel.
    ObjectFactory factory;
    factory.SetTypeId (tokens[3]);
    if(!tokens[2].compare("cpu")) {
        newPEU = CreateObjectWithAttributes<CPU> ("frequency", UintegerValue(freq),
                "name", StringValue("cpu"));
        hwModel->cpu = newPEU->GetObject<CPU>();
        newPEU->taskScheduler = factory.Create()->GetObject<TaskScheduler> ();
    } else { 
        // All other types of PEUs are treated the same
        newPEU = CreateObjectWithAttributes<PEU> ("frequency", UintegerValue(freq),
                "name", StringValue(tokens[2]));
        hwModel->m_PEUs[tokens[3]] = newPEU;
        newPEU->taskScheduler = factory.Create()->GetObject<TaskScheduler> ();
    }

    // Set the taskscheduler via an object factory
    newPEU->hwModel = hwModel;
    newPEU->taskScheduler->Initialize(newPEU);
  }
}

void Processing::HandleConditions(std::vector<std::string> tokens) {
    // Format: TYPE, location/name,[ scope if not loop,] condition name[, condition name 1, ..., condition name n-1 if packet characteristics]
    if(!tokens[0].compare("VCLOC")) {
        locationConditions[tokens[1]].condName = tokens[3];
        locationConditions[tokens[1]].scope = tokens[2].compare("global") ? CONDITIONLOCAL : CONDITIONGLOBAL;
    } else if(!tokens[0].compare("ENQUEUE") || !tokens[0].compare("ENQUEUEN")) {
        struct condition newEnQCond;
        newEnQCond.condName = tokens[3];
        newEnQCond.scope = tokens[2].compare("global") ? CONDITIONLOCAL : CONDITIONGLOBAL;
        enqueueConditions[tokens[1]].push_back(newEnQCond);
    } else if(!tokens[0].compare("DEQUEUE") || !tokens[0].compare("DEQUEUEN")) {
        struct condition newDeQCond;
        newDeQCond.condName = tokens[3];
        newDeQCond.scope = tokens[2].compare("global") ? CONDITIONLOCAL : CONDITIONGLOBAL;
        dequeueConditions[tokens[1]].push_back(newDeQCond);
    } else if(!tokens[0].compare("LOOP")) {
        loopConditions[tokens[1]].condName = tokens[2];
        // A condition on a loop is allways local
        loopConditions[tokens[1]].scope = CONDITIONLOCAL;
    }
}

void Processing::HandleTriggers(std::vector<std::string> tokens) {
    if(!tokens[0].compare("LOC")) {
        locationTriggers[tokens[1]] = tokens[2];
    } else if(!tokens[0].compare("SERVICE")) {
        serviceTriggers[tokens[1]] = tokens[2];
    } else if(!tokens[0].compare("QUEUE")) {
        dequeueTriggers[tokens[1]] = tokens[2];
    }
}

double Processing::stringToDouble( const std::string& s )
{
    std::istringstream i(s);
    double x;
    if (!(i >> x)) {
        NS_FATAL_ERROR("Could not convert string " << s << " to double" << std::endl);
        exit(1);
    }
    return x;
} 

uint32_t Processing::stringToUint32(const std::string& s)
{
    std::istringstream i(s);
    uint32_t x;
    if (!(i >> x)) {
        NS_FATAL_ERROR("Could not convert string " << s << " to uint32_t" << std::endl);
        exit(1);
    }

    return x;
}

void
Processing::HandleSignature(std::vector<std::string> tokens) {
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

    /******************************************/
    /************** SEM HEADER ****************/
    /******************************************/

    if (!tokens[0].compare("NAME")) {
        // Check if this sem allready exists. If not, create it.
        Ptr<ServiceProcessingInfo> spi;
        std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(tokens[1]);

        // It did not exist: create it
        if(it == m_serviceMap.end()) {
            currentlyHandled = Create<SEM> ();
            RegisterSEM(tokens[1], currentlyHandled);
        }

        // It did exist: set currentlyHandled to it
        else
            currentlyHandled = m_serviceMap[tokens[1]]->sem;
    }

    // Just set the PEU in the currently handled SEM
    if(!tokens[0].compare("PEU")) {
        if(!tokens[1].compare("cpu")) {
            currentlyHandled->peu = hwModel->cpu;
        } else
            currentlyHandled->peu = hwModel->m_PEUs[tokens[1]];      
    }

    if(!tokens[0].compare("RESOURCE")) {
        if(!tokens[1].compare("cycles")) 
            currentResources.push_back(CYCLES);
        else if(!tokens[1].compare("milliseconds"))
            currentResources.push_back(MILLISECONDS);
        else if(!tokens[1].compare("instructions"))
            currentResources.push_back(INSTRUCTIONS);
        else if(!tokens[1].compare("cachemisses"))
            currentResources.push_back(CACHEMISSES);
        else if(!tokens[1].compare("memoryaccesses"))
            currentResources.push_back(MEMORYACCESSES);
        else if(!tokens[1].compare("memorystallcycles"))
            currentResources.push_back(MEMSTALLCYCLES);

        // Easier if we just add the string here, and
        // act according to this when handling a PROCESS
        // event below
        currentDistributions.push_back(tokens[2]);      
    }

    if(!tokens[0].compare("FRACTION")) {
        // Do nothing for now
    }

    /**************************************/
    /************** EVENTS ****************/
    /**************************************/

    // When we encounter START, instantiate
    // a new root condition
    if(!tokens[1].compare("START")) {
        currentProgram = new Program();

        // If rootProgram is NULL, this we just
        // created it.
        if(rootProgram == NULL)
            rootProgram = currentProgram;

        // CurrentlyHandled is set when encountering the
        // NAME field in the signature header
        currentProgram->sem = currentlyHandled;
    }

    if (!tokens[1].compare("LOOPSTART")) {
        // This only occurs at the beginnig of service loops now
    }

    if(!tokens[1].compare("STOP")) {    
        // - Iterate through condition/program list
        //   in SEM, adding branches if necessary.
        // - Abort if we encounter inconsistencies.
        // - Note that we need to figure out how
        //   to merge two standard deviations.
        // - Remember to take into consideration the
        //   non-de-queue instances of the loops
        // - Set static vars to NULL

        // Append END-event to current program
        ExecutionEvent *end = new ExecutionEvent();
        end->type = END;
        currentProgram->events.push_back(end);

        if(currentlyHandled->lc != NULL && dequeueOrLoopEncountered) {
            // Here we add it to the queue(s)-not-emtpy SEM
        } else {
            // Here we add it to the queue(s)-empty SEM
        }

        // DEBUG:
        std::cout << "Got signature: " << std::endl;
        Program *curPgm = rootProgram;
        unsigned int i = 0;
        while(i < curPgm->events.size()) {
            ExecutionEvent *curEvt = curPgm->events[i];
            std::cout << curEvt->type;
            if(curEvt->type == CONDITION) {
                Condition *curCnd = ((Condition *) curEvt);
                if(curCnd->condType == LOOPCONDITION &&
                        ((LoopCondition *) curCnd)->emptyQueues != NULL) {
                    curPgm = ((LoopCondition *) curCnd)->emptyQueues;
                }
                else {
                    curPgm = curCnd->programs.front().second;
                }

                i = 0;
            }
            else
                i++;

        }

        std::cout << std::endl;
        std::cout << std::endl;

        return;
    }

    if(!tokens[1].compare("RESTART")){
        // Similar to STOP, but with loops. Implement what is different.
    }

    if(!tokens[1].compare("PROCESS")) {
        // Iterate all HWE aggregates obtained during
        // the parsing of the header.
        // First, create the processing stage
        ProcessingStage *ps = new ProcessingStage();

        // Then, iterate according to all HWE aggregates
        // specified in the header parsed above.
        int numHWEs = currentResources.size();
        int tokenIndex = 0;
        for(int i = 0; i < numHWEs; i++) {
            // Obtain the parameters of the given distribution
            if(!currentDistributions[i].compare("normal")) {
                // The normal distribution takes two parameters:
                // average and standard deviation
                double average = stringToDouble(tokens[2 + tokenIndex++]);
                double sd = stringToDouble(tokens[2 + tokenIndex++]);
                ps->resourcesUsed[currentResources[i]].defined = true;
                ps->resourcesUsed[currentResources[i]].consumption = NormalVariable(average, sd);
            } else if(!currentDistributions[i].compare("lognormal")) {
                // The normal distribution takes two parameters:
                // average and standard deviation
                double logaverage = stringToDouble(tokens[2 + tokenIndex++]);
                double logsd = stringToDouble(tokens[2 + tokenIndex++]);
                ps->resourcesUsed[currentResources[i]].defined = true;
                ps->resourcesUsed[currentResources[i]].consumption = LogNormalVariable(logaverage, logsd);
            }

            //
            // TODO: other distributions - we currently only support normal and lognormal
            // distributions. Lognormal appears to be the better estimator for cycles.
            //
        }

        // Add this PS to the current program
        currentProgram->events.push_back(ps);
    }

    // Handle conditions
    if(!tokens[1].compare("QUEUECOND") ||
            !tokens[1].compare("THREADCOND") ||
            !tokens[1].compare("STATECOND") ||
            !tokens[1].compare("ENQUEUE") ||
            !tokens[1].compare("DEQUEUE") ||
            !tokens[1].compare("PKTEXTR")) {

        Condition *c;
        Program *newProgram = new Program();
        newProgram->sem = currentlyHandled;

        // All conditions are assumed to be local.
        // State-conditions can be used to create
        // global conditions.
        if(!tokens[1].compare("QUEUECOND")) {
            QueueCondition *q = new QueueCondition();
            c = q;
            c->insertEntry(!tokens[4].compare("empty") ? QUEUEEMPTY : QUEUENOTEMPTY,
                    newProgram);
            q->firstQueue = queues[tokens[2]];
            q->firstQueue = queues[tokens[3]];

            std::cout << "Adding CONDITION" << std::endl;
            // Assume local: insert c into current program
            currentProgram->events.push_back(c);
            currentProgram = newProgram;
        } else if (!tokens[1].compare("THREADCOND")) {
            std::cout << "Adding CONDITION" << std::endl;
            ThreadCondition *t = new ThreadCondition();
            c = t;
            c->insertEntry(!tokens[4].compare("ready") ? THREADREADY : THREADNOTREADY,
                    newProgram);
            t->threadId = tokens[2];

            // Assume local: insert c into current program
            currentProgram->events.push_back(c);
            currentProgram = newProgram;
        } else if (!tokens[1].compare("STATECOND") ||
                !tokens[1].compare("PKTEXTR")) {
            std::cout << "Adding CONDITION" << std::endl;
            // Can be global or local
            // First find out if we have a condition specified for this location
            std::map<std::string, struct condition>::iterator foundCond = locationConditions.find(tokens[0]);
            if(foundCond != locationConditions.end()) {
                PacketCharacteristic *pc = new PacketCharacteristic();
                c = pc;

                // Set the packet extraction function id, and insert new program
                c->getCondition = conditionFunctions->conditionMap[foundCond->second.condName];
                c->insertEntry(stringToUint32(tokens[2]), newProgram);

                // Make newProgram the currentProgram after pushing this condition
                currentProgram->events.push_back(c);
                currentProgram = newProgram;	
            }
        } else if (!tokens[1].compare("DEQUEUE") ||
                !tokens[1].compare("DEQUEUEN") ||
                !tokens[1].compare("ENQUEUE") ||
                !tokens[1].compare("ENQUEUEN")) {
            // If queue is 0, we must first find the queue name.
            // But before that, we must confirm that we are in
            // fact within a loop.
            std::string queueName;
            if(!tokens[3].compare("0")){
                if(currentlyHandled->lc == NULL) {
                    NS_FATAL_ERROR("Got queue 0 outside of loop");
                    exit(1);

                    // Elsewise, assume that all queues in the loop
                    // use the same extractor.
                } else {
                    Ptr<Queue> firstQueue = currentlyHandled->lc->firstQueue;

                    // First, find the queue name
                    std::map<Ptr<Queue>, std::string>::iterator name = queueNames.find(firstQueue);
                    if(name != queueNames.end()) {
                        queueName = (*name).second;
                    }
                }
            }

            // Elsewise, we have explicitly specified the queue name. This is
            // quite uncommon, as this means the analyser have not removed the
            // name, and we are de-queuing outside of a loop.
            else
                queueName = tokens[3];

            // Now check if there is a condition on it.
            std::map<std::string, std::vector<struct condition> >::iterator condition =
                !tokens[3].compare("DEQUEUE") || !tokens[3].compare("DEQUEUEN") ?
                dequeueConditions.find(queueName) :
                enqueueConditions.find(queueName);

            // If so, add the condition.
            if(condition != dequeueConditions.end()) {

                // Iterate all conditions on this queue, and chain together
                std::vector<struct condition> allConditions = condition->second;
                struct condition cond;
                bool first = false;
                for(std::vector<struct condition>::iterator it = allConditions.begin();
                        it != allConditions.end(); ++it) {
                    if(!first){
                        newProgram = new Program();
                        first = false;
                    }

                    cond = *it;
                    PacketCharacteristic *pc = new PacketCharacteristic();
                    c = pc;

                    // Set the packet extraction function id, and insert new program
                    c->getCondition = conditionFunctions->conditionMap[cond.condName];
                    c->insertEntry(stringToUint32(tokens[3]), newProgram);

                    // Make newProgram the currentProgram after pushing this condition
                    currentProgram->events.push_back(c);
                    currentProgram = newProgram;
                }
            }
        }
    }

    if(!tokens[1].compare("CALL") ||
            !tokens[1].compare("LOOP")) {
        // - FIRST: check packet structure to add next field (at a later point: intermediate checkpoints)
        // - here we get involved with the triggers. Look at GN1
        //   for the different types of triggers.
        // - note, e.g., that triggers can be "0", meaning we execute
        //   "next" in the packet.
        // - if it regards any of the loops in currentlyHandled->fc,
        //   remember to set dequeueOrLoopEncountered to true.
    }

    if(!tokens[1].compare("ENQUEUE") ||
            !tokens[1].compare("DEQUEUE")) {
        // We are inside of a loop.
        // - if de-queue, remember to set
        //   dequeueOrLoopEncountered to true
        if(currentlyHandled->lc != NULL) {
            dequeueOrLoopEncountered = true;
        }

        // We are outside of a loop
    }

    if(!tokens[1].compare("TTWAKEUP")) {
    }

    if(!tokens[1].compare("SLEEP")) {
    }

    if(!tokens[1].compare("SEMUP") ||
            !tokens[1].compare("SEMDOWN") ||
            !tokens[1].compare("WAITCOMPL") ||
            !tokens[1].compare("COMPL")) {
        // Note that this may be on a tempsynch
    }

    if(!tokens[1].compare("TEMPSYNCH")) {
    }

    if(!tokens[1].compare("LOOP")) {
        // Perhaps the most complex... do last.
    }
}

// Parse device file to create
// - task scheulder
// - SEMs
// - LEUs
// - PEUs
// - Queues
// - Snchronization primitives
    void
Processing::Parse(std::string device)
{
    NS_FATAL_ERROR("Processing::Parse");
    std::ifstream myfile;
    std::string mode;
    myfile.open(device.c_str());
    if (myfile.is_open())
    {
        // Obtain service mapping
        while(myfile.good()) {
            // Get line and tokenize
            std::string line;
            std::getline(myfile,line);
            std::istringstream iss(line);
            std::vector<std::string> tokens;
            std::copy(std::istream_iterator<std::string>(iss),
                    std::istream_iterator<std::string>(),
                    std::back_inserter<std::vector<std::string> >(tokens));

            // If there was a blank line, we know we
            // are done
            if(tokens.size() == 0 || tokens[0].c_str()[0] == '#')
                continue;

            // Mode changes with these keywords
            if(!tokens[0].compare("QUEUES") ||
                    !tokens[0].compare("SYNCH") ||
                    !tokens[0].compare("THREADS") ||
                    !tokens[0].compare("SIGSTART") ||
                    !tokens[0].compare("HARDWARE") ||
                    !tokens[0].compare("CONDITIONS") ||
                    !tokens[0].compare("TRIGGERS")) {

                mode = tokens[0];
                continue;
            }

            // Parse the device header
            if(!mode.compare("QUEUES")) {
                HandleQueue(tokens);
            } else if (!mode.compare("SYNCH")) {
                if(hwModel->cpu->taskScheduler == NULL) {
                    NS_FATAL_ERROR("Attempting to create synchronization variable without having set the task scheduler\n");
                    exit(1);
                }

                HandleSynch(tokens);
            } else if (!mode.compare("THREADS")) {
                HandleThreads(tokens);
            } else if(!mode.compare("HARDWARE")) {
                HandleHardware(tokens);
            } else if (!mode.compare("CONDITIONS")) {
                HandleConditions(tokens);
            } else if (!mode.compare("TRIGGERS")) {
                HandleTriggers(tokens);
            } else if (!mode.compare("SIGSTART")) {
                HandleSignature(tokens);
            } else continue;

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
    }
    else
        std::cout << "Unable to open file" << std::endl;

    myfile.close();
}

/* Current hardcoded only to use packet size as a parameter

TODO: Obtain NetSim values from function in extractorMap
at all this.paramValues. Setting these functions should be
performed by the NetSim service initializer at the same place
where the services themselves are stored

DEPRECATED
*/
// double
// SEM::GetValue(Ptr<Packet> p, Processing *proc)
// {
//   // Find the parameter values between which this
//   // packet's size falls.
//   // If smaller than the smallest, or larger than
//   // the largest, select the smallest and largest
//   // respectively.
//   double minSize = measurements[0][0];
//   double maxSize = measurements.back()[0];
//   uint32_t packetSize = p->GetSize();

//   if(packetSize <= minSize) {
//     if(PROCESSING_DEBUG)
//       std::cout << "Drawing from distribution with " << measurements[0][1] << " " << measurements[0][2] << std::endl;

//     return NormalVariable(measurements[0][1], measurements[0][2], measurements[0][1]).GetValue();
//   }
//   else if (packetSize >= maxSize) {
//     if(PROCESSING_DEBUG)
//       std::cout << "Drawing from distribution with " << measurements.back()[1] << " " << measurements.back()[2] << std::endl;

//     return NormalVariable(measurements.back()[1], measurements.back()[2], measurements.back()[1]).GetValue();
//   }

//   // Find size to the left (smaller) and to the right (larger)
//   // to the current packets size
//   int lIndex = 0, rIndex = 0;
//   double lpsize = 0.0, rpsize = 0.0, lavg = 0.0, ravg = 0.0, lsd = 0.0, rsd = 0.0;
//   int i = 0;
//   for(std::vector<std::vector<double> >::iterator it = measurements.begin(); it != measurements.end(); ++it) {
//     if(packetSize <= (*it)[0]) {
//       rIndex = i;
//       rpsize = (*it)[0];
//       lpsize = measurements[lIndex][0];
//       ravg = (*it)[1];
//       lavg = measurements[lIndex][1];      rsd = (*it)[2];
//       lsd = measurements[lIndex][2];
//       break;
//     }
//     lIndex = i;

//     i++;
//   }

//   // Interpolation factor
//   double iValue = (packetSize - lpsize) / (rpsize - lpsize);

//   // Interpolated avg and sd
//   double iavg = lavg + (ravg - lavg) * iValue;
//   double isd = lsd + (rsd - lsd) * iValue;

//   // DEBUG:

//   if(PROCESSING_DEBUG) {
//     std::cout << "Range: " << minSize << " " << maxSize << std::endl;

//     std::cout
//       << "lIndex" << " "
//       << lIndex << " " << std::endl
//       << "rIndex" << " "
//       << rIndex << " " << std::endl
//       << "lpsize" << " "
//       << lpsize << " " << std::endl
//       << "rpsize" << " "
//       << rpsize << " " << std::endl
//       << "lavg" << " "
//       << lavg << " " << std::endl
//       << "lsd" << " "
//       << lsd << " " << std::endl
//       << "ravg" << " "
//       << ravg << " " << std::endl
//       << "lsd" << " "
//       << rsd << " " << std::endl
//       << "iValue" << " "
//       << iValue << " " << std::endl
//       << "iavg" << " "
//       << iavg << " " << std::endl
//       << "isd" << " "
//       << isd << std::endl;

//     std::cout << "Drawing from distribution with " << iavg << " " << isd << std::endl;
//   }

//   // Return value from normal distribution
//   double sample = NormalVariable(iavg, isd * isd, iavg).GetValue();

//   // Convert to milliseconds according to measurement type
//   switch(resourceType) {
//   case MILLISECONDS:
//     // We want nanoseconds
//     return sample * 1000000;
//   case CPUCYCLES:
//     // GetDuration returns duration in nanoseconds
//     return proc->GetDuration(sample);
//   }
// }

// /* ... and the const version ... */

// double
// SEM::GetValue(Ptr<const Packet> p, Processing *proc)
// {
//   // Find the parameter values between which this
//   // packet's size falls.
//   // If smaller than the smallest, or larger than
//   // the largest, select the smallest and largest
//   // respectively.
//   double minSize = measurements[0][0];
//   double maxSize = measurements.back()[0];
//   uint32_t packetSize = p->GetSize();

//   if(packetSize <= minSize) {
//     if(PROCESSING_DEBUG)
//       std::cout << "Drawing from distribution with " << measurements[0][1] << " " << measurements[0][2] << std::endl;

//     return NormalVariable(measurements[0][1], measurements[0][2], measurements[0][1]).GetValue();
//   }
//   else if (packetSize >= maxSize) {
//     if(PROCESSING_DEBUG)
//       std::cout << "Drawing from distribution with " << measurements.back()[1] << " " << measurements.back()[2] << std::endl;

//     return NormalVariable(measurements.back()[1], measurements.back()[2], measurements.back()[1]).GetValue();
//   }

//   // Find size to the left (smaller) and to the right (larger)
//   // to the current packets size
//   int lIndex = 0, rIndex = 0;
//   double lpsize = 0.0, rpsize = 0.0, lavg = 0.0, ravg = 0.0, lsd = 0.0, rsd = 0.0;
//   int i = 0;
//   for(std::vector<std::vector<double> >::iterator it = measurements.begin(); it != measurements.end(); ++it) {
//     if(packetSize <= (*it)[0]) {
//       rIndex = i;
//       rpsize = (*it)[0];
//       lpsize = measurements[lIndex][0];
//       ravg = (*it)[1];
//       lavg = measurements[lIndex][1];
//       rsd = (*it)[2];
//       lsd = measurements[lIndex][2];
//       break;
//     }
//     lIndex = i;

//     i++;
//   }

//   // Interpolation factor
//   double iValue = (packetSize - lpsize) / (rpsize - lpsize);

//   // Interpolated avg and sd
//   double iavg = lavg + (ravg - lavg) * iValue;
//   double isd = lsd + (rsd - lsd) * iValue;

//   // DEBUG:
//   if(PROCESSING_DEBUG) {

//     std::cout << "Range: " << minSize << " " << maxSize << std::endl;

//     std::cout
//       << "lIndex" << " "
//       << lIndex << " " << std::endl
//       << "rIndex" << " "
//       << rIndex << " " << std::endl
//       << "lpsize" << " "
//       << lpsize << " " << std::endl
//       << "rpsize" << " "
//       << rpsize << " " << std::endl
//       << "lavg" << " "
//       << lavg << " " << std::endl
//       << "lsd" << " "
//       << lsd << " " << std::endl
//       << "ravg" << " "
//       << ravg << " " << std::endl
//       << "lsd" << " "
//       << rsd << " " << std::endl
//       << "iValue" << " "
//       << iValue << " " << std::endl
//       << "iavg" << " "
//       << iavg << " " << std::endl
//       << "isd" << " "
//       << isd << std::endl;

//     std::cout << "Drawing from distribution with " << iavg << " " << isd << std::endl;
//   }

//   // Return value from normal distribution
//   double sample = NormalVariable(iavg, isd * isd, iavg).GetValue();

//   // Convert to milliseconds according to measurement type
//   switch (resourceType) {
//   case MILLISECONDS:
//     // We want nanoseconds
//     return sample * 1000000;
//   case CPUCYCLES:
//     // GetDuration returns duration in nanoseconds
//     return proc->GetDuration(sample);
//   }
// }

// std::map<std::string, Ptr<SEM> > Processing::Parse(std::string file)

// void Processing::Parameterize(std::string file)
// {
//   /* Create SEM map */ 
//   // std::map<std::string, Ptr<SEM> > deviceModel;
//   std::map<std::string, std::string> serviceMap;

//   std::ifstream myfile;
//   myfile.open(file.c_str());
//   if (myfile.is_open())
//   {
//     // Obtain service mapping
//     while(myfile.good()) {
//       // Obtain parameter values and measurements
//       std::string mapline;
//       std::getline(myfile,mapline);
//       std::istringstream iss(mapline);
//       std::vector<std::string> tokens;
//       std::copy(std::istream_iterator<std::string>(iss),
// 		std::istream_iterator<std::string>(),
// 		std::back_inserter<std::vector<std::string> >(tokens));

//       // If there was a blank line, we know we
//       // are done
//       if(tokens.size() == 0)
// 	break;

//       // Make sure we have two, and only two, strings: one stating
//       // the common name, and one stating the name of the real world
//       // function implementing the corresponding service
//       NS_ASSERT(tokens.size() == 2);

//       serviceMap[tokens[1]] = tokens[0];

//       if(PROCESSING_DEBUG)
// 	std::cout << "Bound PHS " << serviceMap[tokens[1]] << " to service " << tokens[1] << std::endl;
//     }

//     while ( myfile.good() )
//     {
//       // Obtain service
//       std::string service;
//       std::getline (myfile,service);

//       // Print service
//       if(PROCESSING_DEBUG)
// 	std::cout << service << ": ";

//       // Create service
//       Ptr<SEM> sem = Create<SEM> ();

//       // Obtain parameters
//       std::string paramline;
//       std::getline (myfile,paramline);
//       std::istringstream iss(paramline);

//       std::vector<std::string> parameters;
//       std::copy(std::istream_iterator<std::string>(iss),
// 		std::istream_iterator<std::string>(),
// 		std::back_inserter<std::vector<std::string> >(parameters));

//       // Store parameters in SEM
//       sem->paramNames = parameters;

//       // Print parameters
//       if(PROCESSING_DEBUG) {
// 	for(std::vector<std::string>::iterator it = parameters.begin(); it != parameters.end(); ++it) {
// 	  std::cout << *it << " ";
// 	}
// 	std::cout << std::endl;
//       }

//       // Obtain measurements
//       std::string measline;
//       std::getline (myfile,measline);
//       std::istringstream iss2(measline);

//       std::vector<std::string> measurements;
//       std::copy(std::istream_iterator<std::string>(iss2),
// 		std::istream_iterator<std::string>(),
// 		std::back_inserter<std::vector<std::string> >(measurements));

//       // Store measurement names in SEM (we assume normal distribution for now, TODO)
//       if(!measurements[0].compare("milliseconds"))
// 	sem->resourceType = MILLISECONDS;
//       else if(!measurements[0].compare("cpucycles"))
// 	sem->resourceType = CPUCYCLES;

//       // Obtain signature (do nothing for now, TODO)
//       std::string signature;
//       std::getline (myfile,signature);

//       while(true) {
// 	// Obtain parameter values and measurements
// 	std::string measline;
// 	std::getline(myfile,measline);
// 	std::istringstream iss(measline);
// 	std::vector<std::string> tokens;
// 	std::copy(std::istream_iterator<std::string>(iss),
// 		  std::istream_iterator<std::string>(),
// 		  std::back_inserter<std::vector<std::string> >(tokens));

// 	// If there was a blank line, we know we
// 	// are done with this service
// 	if(tokens.size() == 0)
// 	  break;

// 	// Print parameter values and measurements
// 	sem->measurements.push_back(std::vector<double> ());
// 	for(std::vector<std::string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
// 	  // Convert to double
// 	  std::istringstream i(*it);
// 	  double paramvalue;
// 	  if (!(i >> paramvalue))
// 	    throw BadConversion("convertToDouble(\"" + *it + "\")");

// 	  if(PROCESSING_DEBUG)
// 	    std::cout << paramvalue << " ";

// 	  // Store measurement in SEM
// 	  sem->measurements.back().push_back(paramvalue);
// 	}
// 	if(PROCESSING_DEBUG)
// 	  std::cout << std::endl;
//       }
//       if(PROCESSING_DEBUG)
// 	std::cout << std::endl;

//       //      deviceModel[service] = sem;

//       if(PROCESSING_DEBUG)
// 	std::cout << "About to register service " <<  service << " at " << serviceMap[service] << std::endl;

//       RegisterSEM(serviceMap[service], sem);
//     }
//     myfile.close();
//   }

//   else std::cout << "Unable to open file" << std::endl;

//   //  return deviceModel;
// }

}
