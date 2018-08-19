#ifndef RAW_DATA_MESSAGE_H
#define RAW_DATA_MESSAGE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "external-ns3-message.h"

using namespace std;

namespace ns3 {

/**
 * Encapsulates a message which is sent from the simulation framework to ns-3 or
 * vice versa which consists only of raw data.
 */
class RawDataMessage : public virtual ExternalNS3Message {
private:
	/**
	 * Initialization method for constructors and assignment operator.
	 * Reserves memory for the data array and copies the data.
	 */
	void init(const uint8_t messageData[], const uint32_t size);

protected:
	/**
	 * Encapsulates the payload of the message as array.
	 */
	uint8_t* messageData;
	/**
	 * Size of the messageData array.
	 */
	uint32_t size;

public:
	RawDataMessage(const uint8_t messageData[], const uint32_t size);

	/**
	 * Copy constructor
	 */
	RawDataMessage(const RawDataMessage& msg);

	/**
	 * Returns the payload data.
	 */
	uint8_t* getMessageData() const;

	/**
	 * Returns the payload size.
	 */
	uint32_t getSize() const;

	virtual void Print (std::ostream &os) const;

	virtual ~RawDataMessage();

	RawDataMessage & operator=(const RawDataMessage& msg);
};

} // namespace ns3

#endif /* RAW_DATA_MESSAGE_H */
