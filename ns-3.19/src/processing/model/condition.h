#ifndef CONDITION_H
#define CONDITION_H

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable.h"
#include "ns3/event-id.h"
#include "ns3/node.h"
#include "ns3/local-state-variable.h"
#include "ns3/local-state-variable-queue.h"
#include "ns3/hwmodel.h"

#include <vector>
#include <stack>
#include <map>
#include <queue>
#include <string>

namespace ns3 {

class Program;
class Packet;
class ExecEnv;
class ProgramLocation;

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

enum conditionOperation {
  CONDITIONREAD,
  CONDITIONWRITE
};

struct condition {
  std::string condName;
  conditionScope scope;
};

 class ConditionFunctions : public Object {
 public:
  static TypeId GetTypeId(void);
  ConditionFunctions();
  void Initialize(Ptr<ExecEnv> execenv);
  Ptr<Node> node;

  std::map<std::string, Callback<uint32_t, Ptr<Thread> > > conditionMap;
  std::map<std::string, Callback<void, Ptr<Thread>, uint32_t > > writeConditionMap;

  // Condition functions working on a packet characteristics and other state
  // variables other than those related to queues and threads
  uint32_t PacketSize(Ptr<Thread> t);
  uint32_t PacketL4Protocol(Ptr<Thread> t);
  uint32_t BCM4329DataOk(Ptr<Thread> t);
  uint32_t Wl1251Intr(Ptr<Thread> t);
  uint32_t ReadWl1251Intr(Ptr<Thread> t);
  uint32_t Wl1251IntrEnabled(Ptr<Thread> t);
  uint32_t Wl1251RxLoop(Ptr<Thread> t);
  uint32_t sizeofnextrxfromnic(Ptr<Thread> t);
  uint32_t ReadNumTxToAck(Ptr<Thread> t);
  void WriteInterruptsEnabled(Ptr<Thread> t, uint32_t value);
  void WriteWl1251Intr(Ptr<Thread> t, uint32_t value);
  void AckNICRx(Ptr<Thread> t, uint32_t value);

  // Conditions working on queues and threads
  uint32_t QueueCondition(Ptr<Queue> first, Ptr<Queue> last);
  uint32_t ServiceQueueCondition(std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *first, std::queue<std::pair<Ptr<SEM>, Ptr<ProgramLocation> > > *last);
  uint32_t ThreadCondition(std::string threadId);
  
  // Used to model wl1251 NIC. Accessed from testprocmod, and thus is set to be global.
  uint32_t numTxToAck;
  uint32_t m_wl1251NICIntrReg;
 private:
};

} // Namespace ns3

#endif /* CONDITION_H */
