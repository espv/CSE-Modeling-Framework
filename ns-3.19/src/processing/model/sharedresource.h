#include "ns3/object.h"

#ifndef SHARED_RESOURCE_H
#define SHARED_RESOURCE_H

namespace ns3 {

class SharedResource : public Object
{
public:
  static TypeId GetTypeId (void);

  SharedResource ();
  SharedResource(int freq);
  virtual ~SharedResource ();

  int Consume(int units);

private:
  int m_freq;
};

} // namespace ns3

#endif
