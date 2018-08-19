#ifndef PROCESSING_H
#define PROCESSING_H

#include <vector>
#include <map>

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

#include "processing-callback.h"
#include "peu.h"
#include "program.h"

//#include "hwmodel.h"

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

#define PROCESSING_DEBUG 0

class ServiceProcessingInfo : public SimpleRefCount<ServiceProcessingInfo>
{
 public:
  Ptr<ProcessingCallbackBase> cb;
  Ptr<SEM> sem;
};

class Processing : public Object
{
public:
  Processing();
  static TypeId GetTypeId (void);

  // The average number of CPU-cycles spent per trace-call
  // This is deducted during ProcessingStage::Instantiate()
  // for the resource "cycles" on the CPU.
  uint32_t m_traceOverhead;

  // Holds functions used to resolve conditions
  Ptr<ConditionFunctions> conditionFunctions;

  // Holds all intra-node queues used during
  // execution.
  std::map<std::string, Ptr<Queue> > queues;
  std::vector<Ptr<Queue> > queueOrder;

  // These are used only during parsing to be able to resolve
  // the queue names when encountering a "0" as a queue id
  std::map<Ptr<Queue>, std::string> queueNames;

  /* Called to register a SEM at a service */
  void RegisterSEM(std::string, Ptr<SEM> s);

  /* Called to parameterize all services according to a device
     description file */
  void Initialize(std::string device);

template<class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1,class T2, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1,class T2, class T3, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1,class T2, class T3, class T4, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1,class T2, class T3, class T4, class T5, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1,class T2, class T3, class T4, class T5, class T6, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1,class T2, class T3, class T4, class T5, class T6, class T7, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);
template<class T1,class T2, class T3, class T4, class T5, class T6, class T7, class T8, class MEM, class OBJ>
  void RegisterService(MEM function, OBJ object, std::string s);

template<class R>
  bool Process(Ptr<Packet> packet, std::string current, std::string next);
template<class T1>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	     T1 arg1);
template<class T1,class T2>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	     T1 arg1, T2 arg2);
template<class T1,class T2, class T3>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	       T1 arg1, T2 arg2, T3 arg3);
template<class T1,class T2, class T3, class T4>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	       T1 arg1, T2 arg2, T3 arg3, T4 arg4);
template<class T1,class T2, class T3, class T4, class T5>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	       T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5);
template<class T1,class T2, class T3, class T4, class T5, class T6>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	       T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6);
template<class T1,class T2, class T3, class T4, class T5, class T6, class T7>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	       T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7);
template<class T1,class T2, class T3, class T4, class T5, class T6, class T7, class T8>
  bool Process(Ptr<Packet> packet, std::string current, std::string next,
	       T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7, T8 arg8);

  // Used to obtain duration given a number of CPU cycles (TODO: add
  // more functions getting as input instruction count, cache misses, etc.,
  // which bases the duration on the current state of shared HW resources).
  double GetDuration(double cpucycles);

  // The hardware model containing:
  // - Interrupt controller
  // - memory bus
  // - all PEUs present in the system
  //     - Which in turn contain threads and taskschedulers
  Ptr<HWModel> hwModel;

  /* Contains all services */
  std::map<std::string, Ptr<ServiceProcessingInfo> > m_serviceMap;

 private:
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

  // To parse device files
  void Parse(std::string device);

  // Variables to hold data regarding manual conditions
  // and triggers. Three types of conditions: manual,
  // queues and loops. The first entry is location,
  // queue name and service(/loop) name, respectively.
  // The second endtry is (a vector of) names to functions
  // providing the value based on NetSim state of packet
  // characteristics.
  //
  // FUTURE WORK: allow for comparisons on values
  enum conditionScope {
    CONDITIONLOCAL,
    CONDITIONGLOBAL
  };
  
  struct condition {
    std::string condName;
    conditionScope scope;
  };

  // Used to store condition information temporarily
  // while parsing signatures
  std::map<std::string, struct condition> locationConditions;
  std::map<std::string, std::vector<struct condition> > dequeueConditions;
  std::map<std::string, std::vector<struct condition> > enqueueConditions;
  std::map<std::string, struct condition> loopConditions;

  // Variables to hold data regarding triggers. Three
  // types of triggers: services, de-queue operations
  // and locations.
  std::map<std::string, std::string> serviceTriggers;
  std::map<std::string, std::string> dequeueTriggers;
  std::map<std::string, std::string> locationTriggers;
};

template<class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<empty,empty,empty,empty,empty,empty,empty,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<empty,empty,empty,empty,empty,empty,empty,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,empty,empty,empty,empty,empty,empty,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,empty,empty,empty,empty,empty,empty,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1,class T2, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,T2,empty,empty,empty,empty,empty,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,T2,empty,empty,empty,empty,empty,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1,class T2, class T3, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,T2,T3,empty,empty,empty,empty,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,T2,T3,empty,empty,empty,empty,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1,class T2, class T3, class T4, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,T2,T3,T4,empty,empty,empty,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,T2,T3,T4,empty,empty,empty,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1,class T2, class T3, class T4, class T5, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,T2,T3,T4,T5,empty,empty,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,T2,T3,T4,T5,empty,empty,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1,class T2, class T3, class T4, class T5, class T6, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,empty,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,empty,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1,class T2, class T3, class T4, class T5, class T6, class T7, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,T7,empty,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,T7,empty,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class T1,class T2, class T3, class T4, class T5, class T6, class T7, class T8, class MEM, class OBJ>
void Processing::RegisterService(MEM function, OBJ object, std::string serviceName) {
  Ptr<ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,T7,T8,MEM,OBJ> >pcf = Create<ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,T7,T8,MEM, OBJ> > ();
  pcf->m_function = function;
  pcf->m_object = object;
  /* Check if entry serviceName allready exists in service map */
  Ptr<ServiceProcessingInfo> spi;
  std::map<std::string, Ptr<ServiceProcessingInfo> >::iterator it = m_serviceMap.find(serviceName);
  if(it == m_serviceMap.end()) {
    m_serviceMap[serviceName] = Create<ServiceProcessingInfo> ();
    std::cout << "xreat" << std::endl;
  }

  /* Insert processing callback into service map */
  spi =  m_serviceMap[serviceName];
  spi->cb = pcf;
}

template<class R>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() << std::endl;


    Ptr<ProcessingCallbackArgs<empty, empty, empty, empty, empty, empty, empty, empty> > pca =
      Create< ProcessingCallbackArgs<empty, empty, empty, empty, empty, empty, empty, empty> > ();
    empty e; pca->SetArgs(e, e, e, e, e, e, e, e);
    packet->m_executionInfo.pcb = pca;

    return true;
  }
}

template<class T1>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() << std::endl;

    Ptr<ProcessingCallbackArgs<T1, empty, empty, empty, empty, empty, empty, empty> > pca =
      Create< ProcessingCallbackArgs<T1, empty, empty, empty, empty, empty, empty, empty> > ();
    empty e; pca->SetArgs(arg1, e, e, e, e, e, e, e);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, empty, empty, empty, empty, empty, empty, empty> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, empty, empty, empty, empty, empty, empty, empty>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, e, e, e, e, e, e, e);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,empty,empty,empty,empty,empty,empty,empty>::run, pca); */

    return true;
  }
}

template<class T1,class T2>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1, T2 arg2)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() <<  std::endl;

    Ptr<ProcessingCallbackArgs<T1, T2, empty, empty, empty, empty, empty, empty> > pca =
      Create< ProcessingCallbackArgs<T1, T2, empty, empty, empty, empty, empty, empty> > ();
    empty e; pca->SetArgs(arg1, arg2, e, e, e, e, e, e);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, T2, empty, empty, empty, empty, empty, empty> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, T2, empty, empty, empty, empty, empty, empty>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, arg2, e, e, e, e, e, e);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,T2,empty,empty,empty,empty,empty,empty>::run, pca); */

    return true;
  }
}

template<class T1,class T2, class T3>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1, T2 arg2, T3 arg3)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() <<  std::endl;

    Ptr<ProcessingCallbackArgs<T1, T2, T3, empty, empty, empty, empty, empty> > pca =
      Create< ProcessingCallbackArgs<T1, T2, T3, empty, empty, empty, empty, empty> > ();
    empty e; pca->SetArgs(arg1, arg2, arg3, e, e, e, e, e);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, T2, T3, empty, empty, empty, empty, empty> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, T2, T3, empty, empty, empty, empty, empty>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, arg2, arg3, e, e, e, e, e);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,T2,T3,empty,empty,empty,empty,empty>::run, pca); */

    return true;
  }
}

template<class T1,class T2, class T3, class T4>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1, T2 arg2, T3 arg3, T4 arg4)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() <<  std::endl;

    Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, empty, empty, empty, empty> > pca =
      Create< ProcessingCallbackArgs<T1, T2, T3, T4, empty, empty, empty, empty> > ();
    empty e; pca->SetArgs(arg1, arg2, arg3, arg4, e, e, e, e);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, empty, empty, empty, empty> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, empty, empty, empty, empty>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, arg2, arg3, arg4, e, e, e, e);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,T2,T3,T4,empty,empty,empty,empty>::run, pca); */

    return true;
  }
}

template<class T1,class T2, class T3, class T4, class T5>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() <<  std::endl;

    Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, empty, empty, empty> > pca =
      Create< ProcessingCallbackArgs<T1, T2, T3, T4, T5, empty, empty, empty> > ();
    empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, e, e, e);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, empty, empty, empty> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, empty, empty, empty>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, e, e, e);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,T2,T3,T4,T5,empty,empty,empty>::run, pca); */

    return true;
  }
}

template<class T1,class T2, class T3, class T4, class T5, class T6>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() <<  std::endl;

    Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, empty, empty> > pca =
      Create< ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, empty, empty> > ();
    empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, arg6, e, e);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, empty, empty> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, empty, empty>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, arg6, e, e);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>::run, pca); */

    return true;
  }
}

  template<class T1,class T2, class T3, class T4, class T5, class T6, class T7>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() <<  std::endl;

    Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, empty> > pca =
      Create< ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, empty> > ();
    empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, arg6, arg7, e);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, empty> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, empty>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, arg6, arg7, e);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::run, pca); */

    return true;
  }
}

  template<class T1,class T2, class T3, class T4, class T5, class T6, class T7, class T8>
  bool Processing::Process(Ptr<Packet> packet, std::string current, std::string next,
			   T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7, T8 arg8)
{
  if(!packet->m_executionInfo.lastProcessed.compare(current))
    return false;
  else {
    packet->m_executionInfo.lastProcessed = current;

    if(PROCESSING_DEBUG)
      std::cout << "Deferring service at " << Simulator::Now() <<  std::endl;

    Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > pca =
      Create< ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > ();
    empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    packet->m_executionInfo.pcb = pca;

    /* Ptr<ProcessingCallbackBase> pcb = m_serviceMap[next]->cb; */
    /* Ptr<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > pca = */
    /*   DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8>, ProcessingCallbackBase> (pcb); */

    /* empty e; pca->SetArgs(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);  */
    /* Simulator::Schedule(NanoSeconds(nanoSecs), &ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::run, pca); */

    return true;
  }
}

} // namespace ns3

#endif
