
#include "cc2420-messages.h"

namespace ns3 {

/* CC2420Cca */
CC2420Cca::CC2420Cca(const bool ccaValue){
	this->ccaValue = ccaValue;
}

void
CC2420Cca::Print (std::ostream &os) const
{
  os << "CC2420Cca: CCA=" << this->ccaValue << " - Medium is " << (this->ccaValue?"free.":"busy.") << endl;
}

bool
CC2420Cca::getCcaValue(){
	return ccaValue;
}


/* CC2420Recv */
CC2420Recv::CC2420Recv(const uint8_t messageData[], const uint32_t size,
		const bool crc, const int32_t rssi) : RawDataMessage(messageData, size) {
	this->crc = crc;
	this->rssi = rssi;
}

void
CC2420Recv::Print (std::ostream &os) const
{
  os << "CC2420Recv: CRC=" << this->crc << " RSSI=" << (int) this->rssi <<
		  " Payload Size=" << this->size << " Data = ";
  for (uint32_t i = 0; i < this->size; i++) {
	  os << "[" << i << "]: " << (int) this->messageData[i] << "  ";
  }
  os << endl;
}

bool
CC2420Recv::getCRC(){
	return crc;
}

int32_t
CC2420Recv::getRSSI(){
	return rssi;
}


/* CC2420SendFinished */
void
CC2420SendFinished::Print (std::ostream &os) const
{
  os << "CC2420SendFinished" << endl;
}


/* CC2420Sending */
CC2420Sending::CC2420Sending(const bool sending){
	this->sending = sending;
}

void
CC2420Sending::Print (std::ostream &os) const
{
  os << "CC2420Sending: Send Successful=" << this->sending << endl;
}

bool
CC2420Sending::getSending(){
	return sending;
}


/* CC2420Send */
CC2420Send::CC2420Send(const uint8_t messageData[], const uint32_t size,
		const bool sendWithCCA) : RawDataMessage(messageData, size) {
	this->sendWithCCA = sendWithCCA;
}


bool
CC2420Send::getSendWithCCA(){
	return sendWithCCA;
}

void
CC2420Send::Print (std::ostream &os) const
{
  os << "CC2420Send: CheckCCA=" << this->sendWithCCA << " Payload Size="
		  << this->size  << " Data = ";
  for (uint32_t i = 0; i < this->size; i++) {
	  os << "[" << i << "]: " << (int) this->messageData[i] << "  ";
  }
  os << endl;
}

/* CC2420Config */
CC2420Config::CC2420Config(const uint8_t cca_mode,
                         const uint8_t cca_hyst,
                         const int8_t cca_thr,
                         const bool tx_turnaround,
                         const bool autocrc,
                         const uint8_t preamble_length,
                         const uint16_t syncword)
{
  this->cca_mode = cca_mode;
  this->cca_hyst = cca_hyst;
  this->cca_thr = cca_thr;
  this->tx_turnaround = tx_turnaround;
  this->autocrc = autocrc;
  this->preamble_length = preamble_length;
  this->syncword = syncword;
}

void
CC2420Config::Print (std::ostream &os) const
{
  os << "CC2420Config: CcaMode=" << (int) this->cca_mode <<
		" CcaHyst=" << (int) this->cca_hyst <<
		" CCA threshold=" << (int) this->cca_thr <<
        " SlowTxTurnAround=" << this->tx_turnaround <<
        " AutoCrc=" << this->autocrc <<
        " PreambleLength=" << (int) this->preamble_length <<
        " SyncWord=0x" << std::hex << (int) this->syncword << std::dec << endl;
}

uint8_t
CC2420Config::getCcaMode()
{
  return cca_mode;
}

uint8_t
CC2420Config::getCcaHysteresis()
{
  return cca_hyst;
}

int8_t
CC2420Config::getCcaThreshold()
{
  return cca_thr;
}

bool
CC2420Config::getTxTurnaround()
{
  return tx_turnaround;
}

bool
CC2420Config::getAutoCrc()
{
  return autocrc;
}

uint8_t
CC2420Config::getPreambleLength()
{
  return preamble_length;
}

uint16_t
CC2420Config::getSyncWord()
{
  return syncword;
}


/* CC2420Setup */
CC2420Setup::CC2420Setup(const uint8_t channel,
                           const uint8_t power)
{
  this->channel = channel;
  this->power = power;
}

void
CC2420Setup::Print (std::ostream &os) const
{
  os << "CC2420Setup: Channel=" << (int) this->channel << " PowerLevel=" << (int) this->power << endl;
}

uint8_t
CC2420Setup::getChannel(){
  return channel;
}

uint8_t
CC2420Setup::getPower()
{
  return power;
}


/* CC2420StatusReq */
void
CC2420StatusReq::Print (std::ostream &os) const
{
  os << "CC242StatusReq" << endl;
}


/* CC2420Status */
CC2420StatusResp::CC2420StatusResp(const uint8_t cca_mode,
						const uint8_t cca_hyst,
						const int8_t cca_thr,
						const uint8_t channel,
						const uint8_t power,
						const bool tx_turnaround,
						const bool autocrc,
						const uint8_t preamble_length,
						const uint16_t syncword)
{
	this->cca_mode = cca_mode;
	this->cca_hyst = cca_hyst;
	this->cca_thr = cca_thr;
	this->channel = channel;
	this->power = power;
	this->tx_turnaround = tx_turnaround;
	this->autocrc = autocrc;
	this->preamble_length = preamble_length;
	this->syncword = syncword;
}

void
CC2420StatusResp::Print (std::ostream &os) const
{
  os << "CC2420Status: CcaMode=" << (int) this->cca_mode <<
		" CcaHyst=" << (int) this->cca_hyst <<
		" CCA threshold=" << (int) this->cca_thr <<
        " Channel=" << (int) this->channel <<
        " PowerLevel=" << (int) this->power <<
        " SlowTxTurnAround=" << this->tx_turnaround <<
        " AutoCrc=" << this->autocrc <<
        " PreambleLength=" << (int) this->preamble_length <<
        " SyncWord=0x" << std::hex << (int) this->syncword << std::dec << endl;
}

uint8_t
CC2420StatusResp::getCcaMode()
{
  return cca_mode;
}

uint8_t
CC2420StatusResp::getCcaHysteresis()
{
  return cca_hyst;
}

int8_t
CC2420StatusResp::getCcaThreshold()
{
  return cca_thr;
}

uint8_t
CC2420StatusResp::getChannel(){
  return channel;
}

uint8_t
CC2420StatusResp::getPower()
{
  return power;
}

bool
CC2420StatusResp::getTxTurnaround()
{
  return tx_turnaround;
}

bool
CC2420StatusResp::getAutoCrc()
{
  return autocrc;
}

uint8_t
CC2420StatusResp::getPreambleLength()
{
  return preamble_length;
}

uint16_t
CC2420StatusResp::getSyncWord()
{
  return syncword;
}

} // Namespace ns3
