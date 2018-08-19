#ifndef MEMBUS_H
#define MEMBUS_H

#include "ns3/object.h"

#include <vector>
#include <stack>
#include <map>
#include <string>

namespace ns3 {

class ProcessingInstance;

class MemBus : public Object
{
public:
  static TypeId GetTypeId (void);

  MemBus();
  MemBus (int contentionModel);
  virtual ~MemBus () {};

  void Contend(ProcessingInstance *pi);

  // freq is public (could be friend) to
  // be accessable by PEUs
  int freq; // Important!: Defined in KHz.

private:
  std::vector<ProcessingInstance *> m_contenders;
};

} // namespace ns3

#endif /* MEMBUS_H */
