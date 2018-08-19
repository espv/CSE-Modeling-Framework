/*
 * LocalStateVariable.h
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#ifndef LOCALSTATEVARIABLE_H_
#define LOCALSTATEVARIABLE_H_

#include <string>
#include <stdint.h>
#include "ns3/simple-ref-count.h"

namespace ns3 {

class StateVariable : public SimpleRefCount <StateVariable> {
public:
	StateVariable();
	virtual ~StateVariable();

	std::string name;
	uint32_t value;
};

}

#endif /* LOCALSTATEVARIABLE_H_ */
