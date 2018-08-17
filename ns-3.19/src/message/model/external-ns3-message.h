#ifndef EXTERNAL_NS3_MESSAGE_H
#define EXTERNAL_NS3_MESSAGE_H

#include "ns3/object.h"

namespace ns3 {

/**
 * Encapsulates a message which is sent from the simulation framework to ns-3 or
 * vice versa.
 */
class ExternalNS3Message : public Object {

public:
	virtual void Print (std::ostream &os) const = 0;

	// virtual destructor is needed for dynamic casting
	virtual ~ExternalNS3Message();

};

std::ostream & operator << (std::ostream &os, const ExternalNS3Message &msg);

} // namespace ns3

#endif /* EXTERNAL_NS3_MESSAGE_H */
