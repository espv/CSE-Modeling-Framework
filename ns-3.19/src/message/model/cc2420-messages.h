#ifndef CC2420_MESSAGES_H
#define CC2420_MESSAGES_H

#include "external-ns3-message.h"
#include "raw-data-message.h"


namespace ns3 {

/**
 * Superclass for all messages which are sent to and received by the CC2420 transceiver.
 */
class CC2420Message : public virtual ExternalNS3Message {
public:
	virtual void Print (std::ostream &os) const = 0;
};



/**
 * Encapsulates a CCA message from the CC2420 transceiver, indicating a change of the
 * CCA value.
 */
class CC2420Cca : public CC2420Message {
public:
	CC2420Cca(const bool ccaValue);
	virtual void Print (std::ostream &os) const;
	bool getCcaValue();

private:
	// true if channel is free, false otherwise
	bool ccaValue;
};



/**
 * Reception of a frame by the CC2420 transceiver.
 */
class CC2420Recv : public CC2420Message , public RawDataMessage {
public:
	CC2420Recv(const uint8_t messageData[], const uint32_t size,
			const bool crc, const int32_t rssi);
	virtual void Print (std::ostream &os) const;
	bool getCRC();
	int32_t getRSSI();

private:
	// true if the crc checksum is valid
	bool crc;
	// received signal strength
	int32_t rssi;
};



/**
 * This message is sent when a sending transmission has finished. It has no parameters.
 */
class CC2420SendFinished : public CC2420Message {
public:
	virtual void Print (std::ostream &os) const;
};



/**
 * A Sending(true) message is sent, if a requested sending transmission can start.
 * Otherwise, a Sending(false) message is sent.
 */
class CC2420Sending : public CC2420Message {
public:
	CC2420Sending(const bool sending);
	virtual void Print (std::ostream &os) const;
	bool getSending();

private:
	// true if requested sending transmission could start, false otherwise
	bool sending;
};



/**
 * Send a frame via CC2420. The data to send and the size of the data buffer are given
 * as parameters. The parameter sendWithCCA indicates if the message shall be sent
 * using CCA.
 */
class CC2420Send : public CC2420Message , public RawDataMessage {
public:
	CC2420Send(const uint8_t messageData[], const uint32_t size, const bool sendWithCCA);
	virtual void Print (std::ostream &os) const;
	bool getSendWithCCA();

private:
	// if true, the message is only sent when the channel is free
	// if false, the message is sent without considering CCA
	bool sendWithCCA;
};



/**
 * Configuration of the CC2420 transceiver. Allows configuration of CCA mode,
 * CCA hysteresis, CCA threshold, calibration time for sending, automatic CRC,
 * preamble length and syncword (see CC2420 data sheet).
 */
class CC2420Config : public CC2420Message {
public:
	CC2420Config(const uint8_t cca_mode,
	            const uint8_t cca_hyst,
	            const int8_t cca_thr,           // cca threshold
	            const bool tx_turnaround,       // true -> long calibration
	            const bool autocrc,             // false -> no crc
	            const uint8_t preamble_length,  // value: 0-15
	            const uint16_t syncword         //
	            );
	virtual void Print (std::ostream &os) const;

	uint8_t getCcaMode();
	uint8_t getCcaHysteresis();
	int8_t getCcaThreshold();
	bool getTxTurnaround();
	bool getAutoCrc();
	uint8_t getPreambleLength();
	uint16_t getSyncWord();

private:
	uint8_t cca_mode;
	uint8_t cca_hyst;
	int8_t cca_thr;
	bool tx_turnaround;
	bool autocrc;
	uint8_t preamble_length;
	uint16_t syncword;
};



/**
 * Configuration of the CC2420 transceiver. Allows configuration of channel and
 * transmission power.
 */
class CC2420Setup : public CC2420Message {
public:
	CC2420Setup(const uint8_t channel,          // 16 channels - value: 11-26
              const uint8_t power);
	virtual void Print (std::ostream &os) const;

	uint8_t getChannel();
	uint8_t getPower();

private:
	uint8_t channel;
	uint8_t power;
};



/**
 * This message is sent to get status information from the transceiver.
 * It has no parameters.
 */
class CC2420StatusReq : public CC2420Message {
public:
	virtual void Print (std::ostream &os) const;
};

/**
 * When a CC2420StatusReq is received, this status message is replied.
 */
class CC2420StatusResp : public CC2420Message {
public:
	CC2420StatusResp(const uint8_t cca_mode,
               const uint8_t cca_hyst,
               const int8_t cca_thr,           // cca threshold
               const uint8_t channel,          // 16 channels - value: 11-26
               const uint8_t power,            // configured sending power
               const bool tx_turnaround,       // true -> long calibration
               const bool autocrc,             // false -> no crc
               const uint8_t preamble_length,  // value: 0-15
               const uint16_t syncword		   //
               );
	virtual void Print (std::ostream &os) const;

	uint8_t getCcaMode();
	uint8_t getCcaHysteresis();
	int8_t getCcaThreshold();
	uint8_t getChannel();
	uint8_t getPower();
	bool getTxTurnaround();
	bool getAutoCrc();
	uint8_t getPreambleLength();
	uint16_t getSyncWord();

private:
	uint8_t cca_mode;
	uint8_t cca_hyst;
	int8_t cca_thr;
	uint8_t channel;
	uint8_t power;
	bool tx_turnaround;
	bool autocrc;
	uint8_t preamble_length;
	uint16_t syncword;
};

} // namespace ns3

#endif /* CC2420_MESSAGES_H */
