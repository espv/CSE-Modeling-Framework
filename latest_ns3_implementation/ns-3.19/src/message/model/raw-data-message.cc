
#include "raw-data-message.h"

namespace ns3 {

void
RawDataMessage::init(const uint8_t messageData[], const uint32_t size) {
	this->messageData = (uint8_t*) malloc(size * sizeof(uint8_t));
	memcpy(this->messageData, messageData, size);
	this->size = size;
}

RawDataMessage::RawDataMessage(const uint8_t messageData[], const uint32_t size) {
	// reserve memory and copy data
	init(messageData, size);
}

RawDataMessage::RawDataMessage(const RawDataMessage& msg) {
	// reserve memory and copy data
	init(msg.messageData, msg.size);
}


uint8_t*
RawDataMessage::getMessageData() const {
	return messageData;
}

uint32_t
RawDataMessage::getSize() const {
	return size;
}

void
RawDataMessage::Print (std::ostream &os) const
{
  os << "RawDataMessage: Data = ";
  for (uint32_t i = 0; i < this->size; i++) {
	  // cast to int is necessary, because uint8_t is interpreted as char otherwise
	  os << "[" << i << "]: " << (int) this->messageData[i] << "  ";
  }
  os << endl;

}

RawDataMessage::~RawDataMessage() {
	free(messageData);
}

RawDataMessage &
RawDataMessage::operator=(const RawDataMessage& msg) {
	if (this != &msg) { // only do assignment if different object
		free(messageData); // free reserved memory for array
		// allocate new memory and store data
		init(msg.messageData, msg.size);
	}
	return *this;
}


} // Namespace ns3
