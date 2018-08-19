#ifndef TRIGGER_H
#define TRIGGER_H

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"

#include <vector>
#include <stack>
#include <map>

namespace ns3 {

class Trigger {
 public:
  Trigger();

 private:
};

} // Namespace ns3

#endif /* TRIGGER_H */
