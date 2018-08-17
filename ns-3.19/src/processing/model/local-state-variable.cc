/*
 * LocalStateVariable.cpp
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#include "local-state-variable.h"
#include <iostream>

namespace ns3 {

StateVariable::StateVariable() {
//	std::cout << "Creating local state variable " << this << std::endl;
	value = 0;
}

StateVariable::~StateVariable() {
//	std::cout << "Destroying local state variable " << this << std::endl;
}

}
