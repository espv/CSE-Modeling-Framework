/*
 * local-state-variable-queue.h
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#ifndef LOCAL_STATE_VARIABLE_QUEUE_H_
#define LOCAL_STATE_VARIABLE_QUEUE_H_

#include <queue>
#include "ns3/simple-ref-count.h"


namespace ns3 {

class StateVariableQueue : public SimpleRefCount<StateVariableQueue>{
public:
	StateVariableQueue();
	virtual ~StateVariableQueue();

	std::queue<uint32_t> stateVariableQueue;
	bool empty(void);
};

}
#endif /* LOCAL_STATE_VARIABLE_QUEUE_H_ */
