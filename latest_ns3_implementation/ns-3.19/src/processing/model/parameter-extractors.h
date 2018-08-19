#ifndef PARAMETER_EXTRACTORS_H
#define PARAMETER_EXTRACTORS_H

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable.h"
#include "ns3/event-id.h"

#include <vector>
#include <stack>
#include <map>

namespace ns3 {

class ParameterExtractors {
 public:
  static void RegisterExtractor(std::string id, Callback cb);

 private:
  static std::vector<std::string, Callback<double> > extractors;
}

}

#endif PARAMETER_EXTRACTORS_H
