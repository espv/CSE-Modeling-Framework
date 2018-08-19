/*
 * local-state-variable-queue.cc
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#include "ns3/local-state-variable-queue.h"
#include <iostream>

namespace ns3 {

StateVariableQueue::StateVariableQueue() {
//	std::cout << "Creating local state variable queue " << this << std::endl;
}

StateVariableQueue::~StateVariableQueue() {
//	std::cout << "Destroying local state variable queue " << this << std::endl;
}

bool StateVariableQueue::empty(void) {
	return stateVariableQueue.empty();
}

}
