#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/processing-module.h"
#include "ns3/data-rate.h"

#include <fstream>
#include <iostream>
#include "ns3/gnuplot.h"
#include <string.h>
#include <time.h>
#include <ctime>


using namespace ns3;

//NS_LOG_COMPONENT_DEFINE("TelosB");

namespace ns3 {
    // For debug
    extern bool debugOn;
    extern bool traceOn;
    extern bool withBlockingIO;
}

static uint32_t seed = 3;
static double duration = 1;
static int pps = 53;  // 98 highest for UDP payload 0, 44 highest for UPD payload 80
static bool print = 0;
static bool print2 = 0;
static int packet_size = 88;
static std::string deviceFile = "telosb-min.device";  // Required if we use gdb
static std::string trace_fn = "/home/espen/Master-thesis/scripts/packets-received.txt";



// UDP payload 22 bytes. Start dropping half of packets at pps=74-84. pps=85-96 there is no packet loss.
// pps=97-99 there is increasing packet loss. pps=100-101 there is no packet loss again. pps=102-114 half successfully forwarded. pps=115-* steady decline from 80% to less than 50% at pps=140.

static ProgramLocation *dummyProgramLoc;
// ScheduleInterrupt schedules an interrupt on the node.
// interruptId is the service name of the interrupt, such as HIRQ-123
void ScheduleInterrupt(Ptr<Node> node, Ptr<Packet> packet, const char* interruptId, Time time) {
    Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();

    // TODO: Model the interrupt distribution somehow
    static int cpu = 0;

    dummyProgramLoc = new ProgramLocation();
    dummyProgramLoc->tempvar = tempVar();
    dummyProgramLoc->curPkt = packet;
    dummyProgramLoc->localStateVariables = std::map<std::string, Ptr<StateVariable> >();
    dummyProgramLoc->localStateVariableQueues = std::map<std::string, Ptr<StateVariableQueue> >();

    Simulator::Schedule(time,
            &InterruptController::IssueInterruptWithServiceOnCPU,
            ee->hwModel->m_interruptController,
            cpu, // cpu
            ee->m_serviceMap[interruptId], // HIRQ-123
            dummyProgramLoc);

    // Round robin distribution
    // cpu = (cpu + 1) % 2;
}


class Radio {

};

class MicroController {

};

class MSP430 : MicroController {
    int cpu_speed = 4;
    float active_battery_drain = 1.8;
    float sleep_battery_drain = 0.0001;
public:
    bool reading_packet_into_ram = false;
    bool writing_packet_to_ram = false;
};

class CC2420 : Radio {
public:
    bool low_power_mode = false;
    std::string state = "READY";
    float active_battery_drain = 23;
    float sleep_battery_drain = 0.0001;
    bool rxfifo_overflow = false;
    int bytes_in_rxfifo = 0;
    DataRate datarate;
    bool busy = false;
    int nr_send_recv = 0;
    bool receiving = false;  // Used to check for collision
    bool collision = false;

    CC2420() {
        datarate = DataRate("250kbps");
    }
};

class Battery {
    float capacity_left;
};

int nr_packets_collision_missed = 0;
int nr_rxfifo_flushes = 0;
int nr_packets_dropped_bad_crc = 0;
int nr_packets_forwarded = 0;
int nr_packets_dropped_ip_layer;
int total_intra_os_delay = 0;
int nr_packets_total = 0;
bool firstNodeSending = false;
std::vector<int> forwarded_packets_seqnos;
std::vector<int> time_received_packets;
std::vector<int> all_intra_os_delays;

class Mote {
protected:
    int id;
    Ptr<Node> node;
    // The below are for logging purposes of the mote

    Mote() {
        static int cnt;
        id = cnt++;
    }

public:
    Ptr<Node> GetNode() {
        return node;
    }
};

class TelosB : Mote {
private:
    // Components of the mote
    Battery battery;
    MSP430 mc;
    CC2420 radio;
    int number_forwarded_and_acked = 0;
    int packets_in_send_queue = 0;
    bool receivingPacket = false;
    std::vector<Ptr<Packet> > receive_queue;

public:
    TelosB(Ptr<Node> node) : Mote() {
        TelosB::node = node;
        TelosB::number_forwarded_and_acked = 0;
        TelosB::packets_in_send_queue = 0;
        TelosB::receivingPacket = false;
        //receive_queue.empty();
    }

    Ptr<Node> GetNode() {
        return node;
    }

    int cur_nr_packets_processing = 0;

    // <Packet, function name> queue when spi is busy
    //std::vector<Ptr<Packet> > receive_queue;

    // Models the radio's behavior before the packets are processed by the microcontroller.
    void ReceivePacket(Ptr<Packet> packet) {
        firstNodeSending = false;
        --radio.nr_send_recv;
        packet->m_executionInfo.timestamps.push_back(Simulator::Now());
        packet->collided = radio.collision;
        if (radio.collision && radio.nr_send_recv == 0)
            radio.collision = false;

        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (radio.rxfifo_overflow) {
            if (print2)
                std::cout << "Dropping packet " << packet->m_executionInfo.seqNr << " due to RXFIFO overflow" << std::endl;
            if (print)
                std::cout << "Dropping packet " << packet->m_executionInfo.seqNr << " due to RXFIFO overflow" << std::endl;
            return;
        }

        if (++cur_nr_packets_processing == 1) {
            if (!execenv->serviceQueues["softirq::gotosleep"]->size () > 0) {
                //execenv->serviceQueues["softirq::gotosleep"]->pop ();
            }
            ScheduleInterrupt (node, packet, "HIRQ-11", Seconds(0));
        }
        //std::cout << "cur_nr_packets_processing: " << cur_nr_packets_processing << std::endl;

        radio.bytes_in_rxfifo += packet->GetSize () + 36;
        if (print) {
            std::cout << "radio.bytes_in_rxfifo: " << radio.bytes_in_rxfifo << std::endl;
            std::cout << "packet->GetSize(): " << packet->GetSize () << std::endl;
        }
       if (radio.bytes_in_rxfifo > 128) {
            radio.bytes_in_rxfifo -= packet->GetSize () + 36;
            if (print)
                std::cout<< id << " RXFIFO overflow" << std::endl;
            packet->collided = true;
            // RemoveAtEnd removes the number of bytes from the received packet that were not received due to overflow.
            packet->RemoveAtEnd(radio.bytes_in_rxfifo - 128);
            radio.bytes_in_rxfifo = 128;
            radio.rxfifo_overflow = true;
        }

        if (receivingPacket/* || (packet->collided && !radio.rxfifo_overflow)*/) {
            // Here, we should enqueue the packet into a spi_busy queue that handles both read_done_length and senddone_task requests.
            if (print)
                std::cout << "Adding packet " << packet->m_executionInfo.seqNr << " to receive_queue, length: " << receive_queue.size()+1 << std::endl;
            receive_queue.push_back(packet);
            return;
        }

        if (print) {
            std::cout<< Simulator::Now() << " " << id << ": CC2420ReceivePacket, next step readDoneLength, radio busy " << packet->m_executionInfo.seqNr << std::endl;
        }

        execenv->Proceed(packet, "readdonelength", &TelosB::read_done_length, this, packet);

        ScheduleInterrupt(node, packet, "HIRQ-1", NanoSeconds(10));
        execenv->queues["h1-h2"]->Enqueue(packet);
        //receivingPacket = true;
        //std::cout << "receivingPacket set to true, " << packet->m_executionInfo.seqNr << std::endl;
    }

    void read_done_length(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;
        if (print)
            std::cout<< Simulator::Now() << " " << id << ": readDone_length, next step readDoneFcf " << packet->m_executionInfo.seqNr << std::endl;
        execenv->Proceed(packet, "readdonefcf", &TelosB::readDone_fcf, this, packet);
        execenv->queues["h2-h3"]->Enqueue(packet);
    }

    void readDone_fcf(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (print)
            std::cout<< Simulator::Now() << " " << id << ": readDone_fcf, next step readDonePayload " << packet->m_executionInfo.seqNr << std::endl;
        execenv->Proceed(packet, "readdonepayload", &TelosB::readDone_payload, this, packet);
        execenv->queues["h3-h4"]->Enqueue(packet);
        execenv->queues["h3-bytes"]->Enqueue(packet);
    }

    void readDone_payload(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        radio.bytes_in_rxfifo -= packet->GetSize () + 36;
        if (print2)
            std::cout << "radio.bytes_in_rxfifo: " << radio.bytes_in_rxfifo << std::endl;
        if (radio.rxfifo_overflow && radio.bytes_in_rxfifo <= 0) {
            if (print)
                std::cout<< "RXFIFO gets flushed" << std::endl;
            radio.rxfifo_overflow = false;
            radio.bytes_in_rxfifo = 0;
            nr_rxfifo_flushes++;
        }

        // Packets received and causing RXFIFO overflow get dropped.
        if (packet->collided) {
            cur_nr_packets_processing--;
            if (cur_nr_packets_processing == 0) {
                ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
            }
            //std::cout << "cur_nr_packets_processing: " << cur_nr_packets_processing << std::endl;
            nr_packets_dropped_bad_crc++;
            if (print2)
                std::cout<< Simulator::Now() << " " << id << ": readDone_payload, collision caused packet CRC check to fail, dropping it " << packet->m_executionInfo.seqNr << std::endl;
            if (print)
                std::cout<< Simulator::Now() << " " << id << ": readDone_payload, collision caused packet CRC check to fail, dropping it " << packet->m_executionInfo.seqNr << std::endl;
            if (!receive_queue.empty()) {
                    //std::cout << "!receive_queue.empty() in receiveDone_task, need to read the new packet" << std::endl;
              Ptr<Packet> nextPacket = receive_queue.front();
              receive_queue.erase(receive_queue.begin());
              execenv->Proceed(nextPacket, "readdonelength", &TelosB::read_done_length, this, nextPacket);
              ScheduleInterrupt(node, nextPacket, "HIRQ-1", Seconds(0));
              execenv->queues["h1-h2"]->Enqueue(nextPacket);
            } else {
                receivingPacket = false;
                //std::cout << "receivingPacket set to false, " << packet->m_executionInfo.seqNr << std::endl;
                if (radio.rxfifo_overflow && radio.bytes_in_rxfifo > 0) {
                    if (print)
                        std::cout<< "RXFIFO gets flushed" << std::endl;
                    radio.rxfifo_overflow = false;
                    radio.bytes_in_rxfifo = 0;
                    nr_rxfifo_flushes++;
                }
            }
        } else {
            if (print)
                std::cout << "readDone_payload seqno: " << packet->m_executionInfo.seqNr << std::endl;
            execenv->Proceed(packet, "receivedone", &TelosB::receiveDone_task, this, packet);
            execenv->queues["h4-rcvd"]->Enqueue(packet);
        }

        if (print)
            std::cout << Simulator::Now() << " " << id << ": readDone_payload " << packet->m_executionInfo.seqNr << ", receivingPacket: " << receivingPacket << ", packet collided: " << packet->collided << std::endl;
    }

    bool jitterExperiment = false;

    void receiveDone_task(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;
        if (print2)
            std::cout << "receiveDone_task" << std::endl;
        if (print)
            std::cout << "packets_in_send_queue: " << packets_in_send_queue << std::endl;

        if (jitterExperiment && packets_in_send_queue < 3) {
            /* In the jitter experiment, we fill the IP layer queue up by enqueueing the same packet three times instead of once.
             * That means we must increase the number of packets getting processed, which depends on how many packets are currently in the send queue.
             */
            cur_nr_packets_processing += 2 - packets_in_send_queue;
            bool first = true;
            while (packets_in_send_queue < 3) {
                ++packets_in_send_queue;
                execenv->queues["send-queue"]->Enqueue(packet);
                execenv->queues["rcvd-send"]->Enqueue(packet);
                if (first) {
                    ScheduleInterrupt(node, packet, "HIRQ-14", MicroSeconds(1));  // Problem with RXFIFO overflow with CCA off might be due to sendTask getting prioritized. IT SHOULD DEFINITELY NOT GET PRIORITIZED. Reading packets from RXFIFO is prioritized.
                    execenv->Proceed(packet, "sendtask", &TelosB::sendTask, this);
                    if (print)
                        std::cout<< Simulator::Now() << " " << id << ": receiveDone " << packet->m_executionInfo.seqNr << std::endl;
                    execenv->queues["ip-bytes"]->Enqueue(packet);
                    first = false;
                }
            }
        } else if (packets_in_send_queue < 3) {
            ++packets_in_send_queue;
            execenv->queues["send-queue"]->Enqueue(packet);
            execenv->queues["rcvd-send"]->Enqueue(packet);
            ScheduleInterrupt(node, packet, "HIRQ-14", MicroSeconds(1));  // Problem with RXFIFO overflow with CCA off might be due to sendTask getting prioritized. IT SHOULD DEFINITELY NOT GET PRIORITIZED. Reading packets from RXFIFO is prioritized.
            execenv->Proceed(packet, "sendtask", &TelosB::sendTask, this);
            if (print)
                std::cout<< Simulator::Now() << " " << id << ": receiveDone " << packet->m_executionInfo.seqNr << std::endl;
            execenv->queues["ip-bytes"]->Enqueue(packet);
        } else {
            cur_nr_packets_processing--;
            if (cur_nr_packets_processing == 0) {
                ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
            }
            //std::cout << "cur_nr_packets_processing: " << cur_nr_packets_processing << std::endl;
            ++nr_packets_dropped_ip_layer;
            ScheduleInterrupt(node, packet, "HIRQ-17", MicroSeconds(1));
            if (print)
                std::cout<< Simulator::Now() << " " << id << ": receiveDone_task, queue full, dropping packet " << packet->m_executionInfo.seqNr << std::endl;
        }

        if (!receive_queue.empty()) {
                //std::cout << "!receive_queue.empty() in receiveDone_task, need to read the new packet" << std::endl;
            Ptr<Packet> nextPacket = receive_queue.front();
            receive_queue.erase(receive_queue.begin());
            execenv->Proceed(nextPacket, "readdonelength", &TelosB::read_done_length, this, nextPacket);
            ScheduleInterrupt(node, nextPacket, "HIRQ-1", Seconds(0));
            execenv->queues["h1-h2"]->Enqueue(nextPacket);
        } else {
            receivingPacket = false;
            //std::cout << "receivingPacket set to false, " << packet->m_executionInfo.seqNr << std::endl;
            if (radio.rxfifo_overflow && radio.bytes_in_rxfifo > 0) {
                if (print)
                    std::cout<< "RXFIFO gets flushed" << std::endl;
                radio.rxfifo_overflow = false;
                radio.bytes_in_rxfifo = 0;
                nr_rxfifo_flushes++;
            }
        }

        if (print2)
            std::cout << "receiveDone_task seqno: " << packet->m_executionInfo.seqNr << std::endl;
    }

    int tx_fifo_queue = 0;
    bool ip_radioBusy = false;

    void sendTask() {
        // TODO: Reschedule this event if a packet can get read into memory. It seems that events run in parallell when we don't want them to.
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        if (execenv->queues["send-queue"]->IsEmpty()) {
            if (print)
                std::cout<< "There are no packets in the send queue, returning from sendTask" << std::endl;
            return;
        }


        if (ip_radioBusy) {
            // finishedTransmitting() calls this function again when ip_radioBusy is set to false.
            if (print)
                std::cout<< "ip_radioBusy is true, returning from sendTask" << std::endl;
            return;
        }

        Ptr<Packet> packet = execenv->queues["send-queue"]->Dequeue();
        ScheduleInterrupt(node, packet, "HIRQ-81", Seconds(0));

        packet->m_executionInfo.executedByExecEnv = false;

        execenv->Proceed(packet, "senddone", &TelosB::sendDoneTask, this, packet);
        execenv->queues["send-senddone"]->Enqueue(packet);

        // The MCU will be busy copying packet from RAM to buffer for a while. Temporary workaround since we cannot schedule MCU to be busy for a dynamic amount of time.
        // 0.7 is a temporary way of easily adjusting the time processing the packet takes.
        execenv->queues["send-bytes"]->Enqueue(packet);
        if (print)
            std::cout<< Simulator::Now() << " " << id << ": sendTask " << packet->m_executionInfo.seqNr << std::endl;

        ip_radioBusy = true;
    }

    bool ccaOn = true;
    bool fakeSending = false;
    int number_forwarded = 0;
    // Called when done writing packet into TXFIFO, and radio is ready to send
    void sendDoneTask(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (!packet->attemptedSent) {
            packet->attemptedSent = true;
            packet->m_executionInfo.timestamps.push_back(Simulator::Now());
            int intra_os_delay = packet->m_executionInfo.timestamps[2].GetMicroSeconds() - packet->m_executionInfo.timestamps[1].GetMicroSeconds();
            time_received_packets.push_back (packet->m_executionInfo.timestamps[1].GetMicroSeconds());
            forwarded_packets_seqnos.push_back (packet->m_executionInfo.seqNr);
            all_intra_os_delays.push_back(intra_os_delay);
            if (print2)
                std::cout << intra_os_delay << " " << packet->m_executionInfo.seqNr << " - # packets forwarded: " << number_forwarded++ << std::endl;
           // std::cout<< id << " sendDoneTask: DELTA: " << intra_os_delay << ", UDP payload size (36+payload bytes): " << packet->GetSize () << std::endl;
            total_intra_os_delay += intra_os_delay;
            if (print) {
                std::cout<< Simulator::Now() << " " << id << ": sendDoneTask " << packet->m_executionInfo.seqNr << std::endl;
                std::cout<< id << " sendDoneTask: DELTA: " << intra_os_delay << ", UDP payload size (36+payload bytes): " << packet->GetSize () << ", seq no " << packet->m_executionInfo.seqNr << std::endl;
                std::cout<< Simulator::Now() << " " << id << ": sendDoneTask, number forwarded: " << ++number_forwarded_and_acked << ", seq no " << packet->m_executionInfo.seqNr << std::endl;
            }
        }

        // DO NOT SEND
        if (fakeSending) {
          ++radio.nr_send_recv;
          Simulator::Schedule(Seconds(0), &TelosB::finishedTransmitting, this, packet);
          return;
        }

        // This is completely conceptual. sendDoneTask doesn't do anything related to this.
        if (radio.nr_send_recv > 0) {
            if (ccaOn) {  // 2500 comes from traces
                Simulator::Schedule(MicroSeconds(2400 + rand() % 200), &TelosB::sendDoneTask, this, packet);
                return;
            }
            radio.collision = true;
            if (print)
                std::cout << "Forwarding packet " << packet->m_executionInfo.seqNr << " causes collision" << std::endl;
        }

        Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize ()+36 + 5/* 36 is UDP packet, 5 is preamble + SFD*/) + MicroSeconds (192) /* 12 symbol lengths before sending packet, even without CCA. 8 symbol lengths is 128 µs */, &TelosB::finishedTransmitting, this, packet);
        ++radio.nr_send_recv;
    }

    // Radio is finished transmitting packet, and packet can now be removed from the send queue as there is no reason to ever re-transmit it.
    // If acks are enabled, the ack has to be received before that can be done.
    void finishedTransmitting(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        ++nr_packets_forwarded;

        // I believe it's here that the packet gets removed from the send queue, but it might be in sendDoneTask
        ip_radioBusy = false;
        packet->m_executionInfo.timestamps.push_back(Simulator::Now());
        if (print)
            std::cout << Simulator::Now() << " " << id << ": finishedTransmitting: DELTA: " << packet->m_executionInfo.timestamps[3] - packet->m_executionInfo.timestamps[0] << ", UDP payload size: " << packet->GetSize () << ", seq no: " << packet->m_executionInfo.seqNr << std::endl;
        --packets_in_send_queue;
        --radio.nr_send_recv;
        if (--cur_nr_packets_processing == 0) {
            //std::cout << "Putting thread to sleep" << std::endl;
            ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
        }
        //std::cout << "cur_nr_packets_processing: " << cur_nr_packets_processing << std::endl;

        if (radio.collision) {
            if (print)
                std::cout << Simulator::Now() << " finishedTransmitting: Collision occured, destroying packet to be forwarded, radio.nr_send_recv: " << radio.nr_send_recv << ", receivingPacket: " << receivingPacket << std::endl;
            if (radio.nr_send_recv == 0) {
                radio.collision = false;
            }
        }

        // In the jitter experiment, we send the same packet three times.
        if (jitterExperiment) {
            packet->attemptedSent = false;
            packet->m_executionInfo.timestamps.pop_back ();
            packet->m_executionInfo.timestamps.pop_back ();
        }

        // Re-scheduling sendTask in case there is a packet waiting to be sent
        execenv->Proceed(packet, "sendtask", &TelosB::sendTask, this);
        execenv->queues["rcvd-send"]->Enqueue(packet);
        ScheduleInterrupt(node, packet, "HIRQ-6", NanoSeconds(0));
    }

    void SendPacket(Ptr<Packet> packet, TelosB *to_mote, TelosB *third_mote) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        if (print)
            std::cout<< Simulator::Now() << " " << id << ": SendPacket " << packet->m_executionInfo.seqNr << std::endl;

        // Finish this, also change ReceivePacket to also accept acks
        if (true || !to_mote->radio.rxfifo_overflow && to_mote->radio.nr_send_recv == 0) {
            if (false && firstNodeSending) {
                Simulator::Schedule(MicroSeconds(100), &TelosB::SendPacket, this, packet, to_mote, third_mote);
                return;
            }

            firstNodeSending = true;
            ++nr_packets_total;
            ++to_mote->radio.nr_send_recv;
            packet->m_executionInfo.timestamps.push_back(Simulator::Now());
            Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize ()+36 + 5/* 36 is UDP packet, 5 is preamble + SFD*/) + MicroSeconds (192) /* 12 symbol lengths before sending packet, even without CCA. 8 symbol lengths is 128 µs */, &TelosB::ReceivePacket, to_mote, packet);
            if (print)
                std::cout << "SendPacket, sending packet " << packet->m_executionInfo.seqNr << std::endl;
        } else if (/*to_mote->radio.nr_send_recv > 0 && */to_mote->radio.nr_send_recv > 0) {
            if (ccaOn) {
                std::cout << "CCA, delaying sending packet" << std::endl;
                Simulator::Schedule(MicroSeconds(2400 + rand() % 200), &TelosB::SendPacket, this, packet, to_mote, third_mote);
                return;
            }
            ++nr_packets_total;
            to_mote->radio.collision = true;
            ++nr_packets_collision_missed;
            // We should send a packet here, but drop it immediately afterwards. The reason why
            // is that this packet's header will not be read by the receiving radio, and thus
            // it will only serve as disturbance or preamble.
            //++to_mote->radio.nr_send_recv;
            //packet->m_executionInfo.timestamps.push_back(Simulator::Now());
            //Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize () + 36/* 36 is UDP packet, 13 is just a constant time before OS gets packet*/), &TelosB::ReceivePacket, to_mote, packet);
        } else { // When our mote is already transmitting a packet, this happens. However, this mote won't know that
                 // our mote is busy transmitting, so this mote will send the packet, and our mote might receive half of the packet for instance.
                 // That would most likely cause garbage to get collected in RXFIFO, which causes overhead for our mote, because it has
                 // to read all the bytes one by one.
            if (print)
                std::cout<< "SendPacket, failed to send because radio's RXFIFO is overflowed" << std::endl;
        }
    }
};

class ProtocolStack {
public:
	void GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TelosB *mote1, TelosB *mote2, TelosB *mote3);
	void GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TelosB *mote1, TelosB *mote2, TelosB *mote3);
	void GeneratePacket(Ptr<Node> n, uint32_t pktSize, uint32_t curSeqNr, TelosB *mote1, TelosB *mote2, TelosB *mote3);
};

Gnuplot *ppsPlot = NULL;
Gnuplot *delayPlot = NULL;
Gnuplot *powerConsumptionPlot = NULL;
Gnuplot *numberForwardedPlot = NULL;
Gnuplot *packetOutcomePlot = NULL;
Gnuplot *numberBadCrcPlot = NULL;
Gnuplot *numberRxfifoFlushesPlot = NULL;
Gnuplot *numberCollidedPlot = NULL;
Gnuplot *numberIPDroppedPlot = NULL;
Gnuplot *intraOsDelayPlot = NULL;
Gnuplot2dDataset *ppsDataSet = NULL;
Gnuplot2dDataset *delayDataSet = NULL;
Gnuplot2dDataset *numberForwardedDataSet = NULL;
Gnuplot2dDataset *numberForwardedDataSet2 = NULL;
Gnuplot2dDataset *numberBadCrcDataSet = NULL;
Gnuplot2dDataset *numberRxfifoFlushesDataSet = NULL;
Gnuplot2dDataset *numberCollidedDataSet = NULL;
Gnuplot2dDataset *numberIPDroppedDataSet = NULL;
Gnuplot2dDataset *intraOsDelayDataSet = NULL;
Gnuplot2dDataset *powerConsumptionDataSet = NULL;

void createPlot(Gnuplot** plot, std::string filename, std::string title, Gnuplot2dDataset** dataSet) {
    *plot = new Gnuplot(filename);
    (*plot)->SetTitle(title);
    (*plot)->SetTerminal("png");

    *dataSet = new Gnuplot2dDataset();
    (*dataSet)->SetTitle(title);
    (*dataSet)->SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void createPlot2(Gnuplot** plot, std::string filename, std::string title, Gnuplot2dDataset** dataSet, std::string dataSetTitle) {
    *plot = new Gnuplot(filename);
    (*plot)->SetTitle(title);
    (*plot)->SetTerminal("png");

    *dataSet = new Gnuplot2dDataset();
    (*dataSet)->SetTitle(dataSetTitle);
    (*dataSet)->SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void writePlot(Gnuplot* plot, std::string filename, Gnuplot2dDataset* dataSet) {
    plot->AddDataset(*dataSet);
    std::ofstream plotFile(filename.c_str());
    plot->GenerateOutput(plotFile);
    plotFile.close();

    //delete plot;
    //delete dataSet;
}

void writePlot2Lines(Gnuplot* plot, std::string filename, Gnuplot2dDataset* dataSet1, Gnuplot2dDataset* dataSet2) {
    plot->AddDataset(*dataSet1);
    plot->AddDataset(*dataSet2);
    std::ofstream plotFile(filename.c_str());
    plot->GenerateOutput(plotFile);
    plotFile.close();

    //delete plot;
    //delete dataSet;
}

void writePlot3Lines(Gnuplot* plot, std::string filename, Gnuplot2dDataset* dataSet1, Gnuplot2dDataset* dataSet2, Gnuplot2dDataset* dataSet3) {
    plot->AddDataset(*dataSet1);
    plot->AddDataset(*dataSet2);
    plot->AddDataset(*dataSet3);
    std::ofstream plotFile(filename.c_str());
    plot->GenerateOutput(plotFile);
    plotFile.close();

    //delete plot;
    //delete dataSet;
}

void writePlot4Lines(Gnuplot* plot, std::string filename, Gnuplot2dDataset* dataSet1, Gnuplot2dDataset* dataSet2, Gnuplot2dDataset* dataSet3, Gnuplot2dDataset* dataSet4) {
    plot->AddDataset(*dataSet1);
    plot->AddDataset(*dataSet2);
    plot->AddDataset(*dataSet3);
    plot->AddDataset(*dataSet4);
    std::ofstream plotFile(filename.c_str());
    plot->GenerateOutput(plotFile);
    plotFile.close();

    //delete plot;
    //delete dataSet;
}

// GeneratePacket creates a packet and passes it on to the NIC
void ProtocolStack::GeneratePacket(Ptr<Node> n, uint32_t pktSize, uint32_t curSeqNr, TelosB *mote1, TelosB *mote2, TelosB *mote3) {
    if (print2)
        std::cout << "GeneratePacket packet size: " << pktSize << std::endl;
    Ptr<Packet> toSend = Create<Packet>(pktSize);
        toSend->m_executionInfo.seqNr = curSeqNr;
        toSend->m_executionInfo.executedByExecEnv = false;

        Ptr<ExecEnv> execenv = n->GetObject<ExecEnv>();

    if (print)
        std::cout<< "Generating packet " << curSeqNr << std::endl;

    mote1->SendPacket(toSend, mote2, mote3);
}

// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate.
void ProtocolStack::GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TelosB *mote1, TelosB *mote2, TelosB *mote3) {
    static int curSeqNr = 0;

    GeneratePacket(n, pktSize, curSeqNr++, mote1, mote2, mote3);
        if (Simulator::Now().GetSeconds() + (1.0 / (double) pps) < duration - 0.02)
                Simulator::Schedule(Seconds(1.0 / (double) pps) + MicroSeconds(rand() % 100),
                &ProtocolStack::GenerateTraffic, this, n, /*rand()%(80 + 1)*/pktSize, mote1, mote2, mote3);
}


int cnt = 0;
// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate.
void ProtocolStack::GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TelosB *mote1, TelosB *mote2, TelosB *mote3) {
        static int curSeqNr = 0;

   // Simulator::Schedule(time, &ProtocolStack::GeneratePacket, this, n, pktSize, curSeqNr++, mote1, mote2, mote3);
    Simulator::Schedule(time,
      &ProtocolStack::GenerateTraffic, this, n, /*rand()%(80 + 1)*/pktSize, mote1, mote2, mote3);
}

int main(int argc, char *argv[])
{
    // Debugging and tracing
    ns3::debugOn = false;
    ns3::traceOn = false;
    ns3::withBlockingIO = true;

    // Fetch from command line
    CommandLine cmd;
    cmd.AddValue("seed", "seed for the random generator", seed);
    cmd.AddValue("duration", "The number of seconds the simulation should run", duration);
    cmd.AddValue("trace", "Trace parsing of device file, and execution of SEM", ns3::traceOn);
    cmd.AddValue("pps", "Packets per second", pps);
    cmd.AddValue("ps", "Packet size", packet_size);
    cmd.AddValue("print", "Print events in the protocol stack", print);
    cmd.AddValue("device", "Device file to use for simulation", deviceFile);
    cmd.AddValue("trace_file", "Trace file including times when packets should get sent", trace_fn);
    cmd.Parse(argc, argv);

    SeedManager::SetSeed(seed);

    createPlot(&ppsPlot, "testplot.png", "pps", &ppsDataSet);
    createPlot(&delayPlot, "delayplot.png", "intra-os delay", &delayDataSet);

#define READ_TRACES 0
#define ONE_CONTEXT 0
#define SIMULATION_OVERHEAD_TEST 0
#define ALL_CONTEXTS 1
#if READ_TRACES
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));

    // Create node with ExecEnv
    NodeContainer c;
    memset(&c, 0, sizeof(NodeContainer));
    c.Create(3);

    std::string line;
    std::ifstream trace_file;
    trace_file.open (trace_fn.c_str());

    eeh->Install(deviceFile, c.Get(0));
    eeh->Install(deviceFile, c.Get(1));
    eeh->Install(deviceFile, c.Get(2));

    Ptr<ExecEnv> ee1 = c.Get(0)->GetObject<ExecEnv>();
    Ptr<ExecEnv> ee2 = c.Get(1)->GetObject<ExecEnv>();
    Ptr<ExecEnv> ee3 = c.Get(2)->GetObject<ExecEnv>();
    ProtocolStack *protocolStack = new ProtocolStack();

    TelosB *mote1 = new TelosB(c.Get(0));
    ScheduleInterrupt (mote1->GetNode(), Create<Packet>(0), "HIRQ-12", Seconds(0));
    TelosB *mote2 = new TelosB(c.Get(1));
    ScheduleInterrupt (mote2->GetNode(), Create<Packet>(0), "HIRQ-12", Seconds(0));
    TelosB *mote3 = new TelosB(c.Get(2));
    ScheduleInterrupt (mote3->GetNode(), Create<Packet>(0), "HIRQ-12", Seconds(0));

    print = true;
    print2 = false;
    pps = 0;  // Need to disable pps here
    bool next_is_packet_size = false;
    Time first_time = MicroSeconds(0);
    Time next_time;
    while (!trace_file.eof()) {
        getline(trace_file, line);
        if (next_is_packet_size) {
          next_is_packet_size = false;
          packet_size = atoi(line.c_str());
        } else {
          next_is_packet_size = true;
          next_time = MicroSeconds(atoi(line.c_str()));
          if (first_time == MicroSeconds(0))
            first_time = next_time;
          continue;
        }
        protocolStack->GenerateTraffic2(c.Get(0), packet_size-36, MicroSeconds ((next_time-first_time).GetMicroSeconds()*0.87), mote1, mote2, mote3);
        //Simulator::Schedule(MicroSeconds(atoi(line.c_str())),
        //                    &ProtocolStack::GeneratePacket, protocolStack, c.Get(0), packet_size, curSeqNr++, mote1, mote2, mote3);
        std::cout << "Sending packet at " << packet_size-36 << " or in microseconds " << (next_time-first_time).GetMicroSeconds() << std::endl;
    }
    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();

    createPlot(&intraOsDelayPlot, "intraOsDelay.png", "Intra-OS delay", &intraOsDelayDataSet);
    for (int i = 0; i < all_intra_os_delays.size(); ++i) { 
        std::cout << "3 " /*<< i << " " << forwarded_packets_seqnos[i] << " " << time_received_packets[i] << " " */<< all_intra_os_delays[i] << std::endl;
        intraOsDelayDataSet->Add(forwarded_packets_seqnos[i], all_intra_os_delays[i]);
    }
    std::cout << "UDP payload: " << packet_size << ", pps: " << pps << ", RXFIFO flushes: " << nr_rxfifo_flushes << ", bad CRC: " << nr_packets_dropped_bad_crc << ", radio collision: " << nr_packets_collision_missed << ", ip layer drop: " << nr_packets_dropped_ip_layer << ", successfully forwarded: " << nr_packets_forwarded << " / " << nr_packets_total << " = " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "% Intra OS median: " << all_intra_os_delays.at(all_intra_os_delays.size()/2) << std::endl;

    writePlot(intraOsDelayPlot, "plots/intraOsDelay.gnu", intraOsDelayDataSet);

#elif ONE_CONTEXT
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));

    // Create node with ExecEnv
    NodeContainer c;
    memset(&c, 0, sizeof(NodeContainer));
    c.Create(3);

    eeh->Install(deviceFile, c.Get(0));
    eeh->Install(deviceFile, c.Get(1));
    eeh->Install(deviceFile, c.Get(2));

    Ptr<ExecEnv> ee1 = c.Get(0)->GetObject<ExecEnv>();
    Ptr<ExecEnv> ee2 = c.Get(1)->GetObject<ExecEnv>();
    Ptr<ExecEnv> ee3 = c.Get(2)->GetObject<ExecEnv>();
    ProtocolStack *protocolStack = new ProtocolStack();

    TelosB *mote1 = new TelosB(c.Get(0));
    ScheduleInterrupt (mote1->GetNode(), Create<Packet>(0), "HIRQ-12", Seconds(0));
    TelosB *mote2 = new TelosB(c.Get(1));
    ScheduleInterrupt (mote2->GetNode(), Create<Packet>(0), "HIRQ-12", Seconds(0));
    TelosB *mote3 = new TelosB(c.Get(2));
    ScheduleInterrupt (mote3->GetNode(), Create<Packet>(0), "HIRQ-12", Seconds(0));

    print = true;
    protocolStack->GenerateTraffic(c.Get(0), packet_size, mote1, mote2, mote3);
    Simulator::Stop(Seconds(duration));
    clock_t t;
    t = clock();
    Simulator::Run();
    t = clock() - t;
    Simulator::Destroy();
    std::cout << "UDP payload: " << packet_size << ", pps: " << pps << ", RXFIFO flushes: " << nr_rxfifo_flushes << ", bad CRC: " << nr_packets_dropped_bad_crc << ", radio collision: " << nr_packets_collision_missed << ", ip layer drop: " << nr_packets_dropped_ip_layer << ", successfully forwarded: " << nr_packets_forwarded << " / " << nr_packets_total << " = " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "%" << std::endl;
    std::cout << "1 " << packet_size << " " << pps << " " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "\n";
    std::cout << "2 " << packet_size << " " << pps << " " << (nr_packets_dropped_ip_layer/(float)nr_packets_total)*100 << "\n";
    std::cout << "3 " << packet_size << " " << pps << " " << total_intra_os_delay/(float)nr_packets_total/*all_intra_os_delays.at(all_intra_os_delays.size()/2)*/ << "\n";
    std::cout << "Milliseconds it took to simulate: " << t << std::endl;
#elif SIMULATION_OVERHEAD_TEST
    NodeContainer c;
    int numberMotes = 10;
    pps = 0;
    duration = 1;
    memset(&c, 0, sizeof(NodeContainer));
    c.Create(numberMotes);

    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    /*Ptr<ExecEnvHelper> eeh2 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh3 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh4 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh5 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh6 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh7 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh8 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh9 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh10 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));

    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));*/

   // eeh1->Install(deviceFile, c.Get(10));
    /*eeh2->Install(deviceFile, c.Get(11));
    eeh3->Install(deviceFile, c.Get(12));
    eeh4->Install(deviceFile, c.Get(13));
    eeh5->Install(deviceFile, c.Get(14));
    eeh6->Install(deviceFile, c.Get(15));
    eeh7->Install(deviceFile, c.Get(16));
    eeh8->Install(deviceFile, c.Get(17));
    eeh9->Install(deviceFile, c.Get(18));
    eeh10->Install(deviceFile, c.Get(19));*/

    ProtocolStack *protocolStack = new ProtocolStack();
    /*ProtocolStack *protocolStack1 = new ProtocolStack();
    ProtocolStack *protocolStack2 = new ProtocolStack();
    ProtocolStack *protocolStack3 = new ProtocolStack();
    ProtocolStack *protocolStack4 = new ProtocolStack();
    ProtocolStack *protocolStack5 = new ProtocolStack();
    ProtocolStack *protocolStack6 = new ProtocolStack();
    ProtocolStack *protocolStack7 = new ProtocolStack();
    ProtocolStack *protocolStack8 = new ProtocolStack();
    ProtocolStack *protocolStack9 = new ProtocolStack();*/

    clock_t install_time;
    install_time = clock();
    eeh->Install(deviceFile, c);
    for (int i = 0; i < numberMotes; i++) {
        ScheduleInterrupt (c.Get(i), Create<Packet>(0), "HIRQ-12", Seconds(0));
        protocolStack->GenerateTraffic(c.Get(i), packet_size, new TelosB(c.Get(i)), new TelosB(c.Get(i)), new TelosB(c.Get(i)));
    }
    install_time = clock() - install_time;

    /*TelosB *moteFrom0 = new TelosB(c.Get(0));
    TelosB *moteFrom1 = new TelosB(c.Get(1));
    TelosB *moteFrom2 = new TelosB(c.Get(2));
    TelosB *moteFrom3 = new TelosB(c.Get(3));
    TelosB *moteFrom4 = new TelosB(c.Get(4));
    TelosB *moteFrom5 = new TelosB(c.Get(5));
    TelosB *moteFrom6 = new TelosB(c.Get(6));
    TelosB *moteFrom7 = new TelosB(c.Get(7));
    TelosB *moteFrom8 = new TelosB(c.Get(8));
    TelosB *moteFrom9 = new TelosB(c.Get(9));

    TelosB *moteInt0 = new TelosB(c.Get(10));
    TelosB *moteInt1 = new TelosB(c.Get(11));
    TelosB *moteInt2 = new TelosB(c.Get(12));
    TelosB *moteInt3 = new TelosB(c.Get(13));
    TelosB *moteInt4 = new TelosB(c.Get(14));
    TelosB *moteInt5 = new TelosB(c.Get(15));
    TelosB *moteInt6 = new TelosB(c.Get(16));
    TelosB *moteInt7 = new TelosB(c.Get(17));
    TelosB *moteInt8 = new TelosB(c.Get(18));
    TelosB *moteInt9 = new TelosB(c.Get(19));

    TelosB *moteTo0 = new TelosB(c.Get(20));
    TelosB *moteTo1 = new TelosB(c.Get(21));
    TelosB *moteTo2 = new TelosB(c.Get(22));
    TelosB *moteTo3 = new TelosB(c.Get(23));
    TelosB *moteTo4 = new TelosB(c.Get(23));
    TelosB *moteTo5 = new TelosB(c.Get(24));
    TelosB *moteTo6 = new TelosB(c.Get(25));
    TelosB *moteTo7 = new TelosB(c.Get(26));
    TelosB *moteTo8 = new TelosB(c.Get(27));
    TelosB *moteTo9 = new TelosB(c.Get(28));*/


    print = false;
    //protocolStack->GenerateTraffic(c.Get(0), packet_size, moteFrom0, moteInt0, moteTo0);
    /*protocolStack1->GenerateTraffic(c.Get(1), 0, moteFrom1, moteInt1, moteTo1);
    protocolStack2->GenerateTraffic(c.Get(2), 0, moteFrom2, moteInt2, moteTo2);
    protocolStack3->GenerateTraffic(c.Get(3), 0, moteFrom3, moteInt3, moteTo3);
    protocolStack4->GenerateTraffic(c.Get(4), 0, moteFrom4, moteInt4, moteTo4);
    protocolStack5->GenerateTraffic(c.Get(5), 0, moteFrom5, moteInt5, moteTo5);
    protocolStack6->GenerateTraffic(c.Get(6), 0, moteFrom6, moteInt6, moteTo6);
    protocolStack7->GenerateTraffic(c.Get(7), 0, moteFrom7, moteInt7, moteTo7);
    protocolStack8->GenerateTraffic(c.Get(8), 0, moteFrom8, moteInt8, moteTo8);
    protocolStack9->GenerateTraffic(c.Get(9), 0, moteFrom9, moteInt9, moteTo9);*/

    /*protocolStack1->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote13, mote2);
    protocolStack2->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote12, mote2);
    protocolStack3->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote11, mote2);
    protocolStack4->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote10, mote2);
    protocolStack5->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote9, mote2);
    protocolStack6->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote8, mote2);
    protocolStack7->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote7, mote2);
    //protocolStack8->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote6, mote2);
    protocolStack9->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote5, mote2);
    protocolStack10->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote4, mote2);
    protocolStack11->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote3, mote2);
    protocolStack12->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote2, mote2);
    protocolStack13->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote1, mote2);
    protocolStack14->GenerateTraffic2(c.Get(0), 0, Seconds(0), mote0, mote0, mote2);*/
    Simulator::Stop(Seconds(duration));
    clock_t t;
    t = clock();
    Simulator::Run();
    t = clock() - t;
    Simulator::Destroy();
    std::cout << "UDP payload: " << packet_size << ", pps: " << pps << ", RXFIFO flushes: " << nr_rxfifo_flushes << ", bad CRC: " << nr_packets_dropped_bad_crc << ", radio collision: " << nr_packets_collision_missed << ", ip layer drop: " << nr_packets_dropped_ip_layer << ", successfully forwarded: " << nr_packets_forwarded << " / " << nr_packets_total << " = " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "%" << std::endl;
    std::cout << "1 " << packet_size << " " << pps << " " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "\n";
    std::cout << "2 " << packet_size << " " << pps << " " << (nr_packets_dropped_ip_layer/(float)nr_packets_total)*100 << "\n";
    std::cout << "3 " << packet_size << " " << pps << " " << total_intra_os_delay/(float)nr_packets_total/*all_intra_os_delays.at(all_intra_os_delays.size()/2)*/ << "\n";
    std::cout << "Microseconds to simulate " << numberMotes << " motes for " << duration << " seconds: " << t << ", install time in microseconds: " << install_time << std::endl;
#elif ALL_CONTEXTS
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));

    // Create node with ExecEnv
    NodeContainer c;
    memset(&c, 0, sizeof(NodeContainer));
    c.Create(3);

    std::ofstream numberForwardedFile ("plots/numberForwardedPoints.txt");
    if (!numberForwardedFile.is_open()) {
        std::cout << "Failed to open numberForwardedPoints.txt, exiting" << std::endl;
        exit(-1);
    }
    for (int i = 88; i >= 0; i-=8) {
        packet_size = i;
        std::ostringstream os;
        os << packet_size;
        createPlot(&numberForwardedPlot, "numberForwarded"+os.str()+".png", "Forwarded at packet size: "+os.str(), &numberForwardedDataSet);
        createPlot2(&packetOutcomePlot, "packetOutcome"+os.str()+".png", "Packet outcome at packet size: "+os.str(), &numberForwardedDataSet2, "Forwarded");
        createPlot(&numberCollidedPlot, "numberCollided"+os.str()+".png", "Collided at packet size: "+os.str(), &numberCollidedDataSet);
        createPlot(&numberRxfifoFlushesPlot, "numberRxfifoFlushes"+os.str()+".png", "RXFIFO flushes at packet size: "+os.str(), &numberRxfifoFlushesDataSet);
        createPlot(&numberBadCrcPlot, "numberBadCrc"+os.str()+".png", "Bad CRC at packet size: "+os.str(), &numberBadCrcDataSet);
        createPlot(&numberIPDroppedPlot, "numberIPdropped"+os.str()+".png", "Dropped at IP layer - packet size: "+os.str(), &numberIPDroppedDataSet);
        createPlot(&intraOsDelayPlot, "intraOsDelay"+os.str()+".png", "Intra-OS delay - packet size: "+os.str(), &intraOsDelayDataSet);
        numberForwardedPlot->AppendExtra ("set xrange [37:]");
        numberCollidedPlot->AppendExtra ("set xrange [37:]");
        numberRxfifoFlushesPlot->AppendExtra ("set xrange [37:]");
        numberBadCrcPlot->AppendExtra ("set xrange [37:]");
        numberIPDroppedPlot->AppendExtra ("set xrange [37:]");

        for (float j = 0; j <= 150; j++) {
            pps = j;

            nr_packets_forwarded = 0;
            nr_packets_total = 0;
            nr_packets_dropped_bad_crc = 0;
            nr_packets_collision_missed = 0;
            nr_packets_dropped_ip_layer = 0;
            nr_rxfifo_flushes = 0;
            total_intra_os_delay = 0;
            all_intra_os_delays.clear();

            // Create node with ExecEnv
            NodeContainer c;
            memset(&c, 0, sizeof(NodeContainer));
            c.Create(3);

            Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
                    "cacheLineSize", UintegerValue(64), "tracingOverhead",
                    UintegerValue(0));

            eeh->Install(deviceFile, c.Get(0));
            eeh->Install(deviceFile, c.Get(1));
            eeh->Install(deviceFile, c.Get(2));

            Ptr<ExecEnv> ee1 = c.Get(0)->GetObject<ExecEnv>();
            Ptr<ExecEnv> ee2 = c.Get(1)->GetObject<ExecEnv>();
            Ptr<ExecEnv> ee3 = c.Get(2)->GetObject<ExecEnv>();
            ProtocolStack *protocolStack = new ProtocolStack();

            TelosB *mote1 = new TelosB(c.Get(0));
            TelosB *mote2 = new TelosB(c.Get(1));
            TelosB *mote3 = new TelosB(c.Get(2));

            protocolStack->GenerateTraffic(c.Get(0), packet_size, mote1, mote2, mote3);
            Simulator::Stop(Seconds(duration));
            Simulator::Run();
            Simulator::Destroy();
            numberForwardedDataSet->Add(pps, (nr_packets_forwarded/(float)nr_packets_total)*100);
            numberForwardedDataSet2->Add(pps, (nr_packets_forwarded/(float)nr_packets_total)*100);
            numberBadCrcDataSet->Add(pps, (nr_packets_dropped_bad_crc/(float)nr_packets_total)*100);
            numberCollidedDataSet->Add(pps, (nr_packets_collision_missed/(float)nr_packets_total)*100);
            numberRxfifoFlushesDataSet->Add(pps, nr_rxfifo_flushes);
            numberIPDroppedDataSet->Add(pps, (nr_packets_dropped_ip_layer/(float)nr_packets_total)*100);
            //intraOsDelayDataSet->Add(pps, total_intra_os_delay/(float)nr_packets_total);
            intraOsDelayDataSet->Add(pps, all_intra_os_delays.at(all_intra_os_delays.size()/2));

            numberForwardedFile << std::flush;
            numberForwardedFile << "1 " << i << " " << pps << " " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "\n";
            numberForwardedFile << std::flush;
            numberForwardedFile << "2 " << i << " " << pps << " " << (nr_packets_dropped_ip_layer/(float)nr_packets_total)*100 << "\n";
            numberForwardedFile << std::flush;
            numberForwardedFile << "3 " << i << " " << pps << " " << all_intra_os_delays.at(all_intra_os_delays.size()/2) << "\n";
            numberForwardedFile << std::flush;

            //writePlot(ppsPlot, "testplot.gnu", ppsDataSet);
            //writePlot(delayPlot, "delay.gnu", delayDataSet);
            std::cout << "UDP payload: " << packet_size << ", pps: " << pps << ", RXFIFO flushes: " << nr_rxfifo_flushes << ", bad CRC: " << nr_packets_dropped_bad_crc << ", radio collision: " << nr_packets_collision_missed << ", ip layer drop: " << nr_packets_dropped_ip_layer << ", successfully forwarded: " << nr_packets_forwarded << " / " << nr_packets_total << " = " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "% Intra OS median: " << all_intra_os_delays.at(all_intra_os_delays.size()/2) << std::endl;
            //memset(mote1, 0, sizeof(TelosB));
            //memset(mote2, 0, sizeof(TelosB));
            //memset(mote3, 0, sizeof(TelosB));
            delete mote1;
            delete mote2;
            delete mote3;
            delete protocolStack;
            memset(eeh, 0, sizeof(ExecEnvHelper));
        }

        writePlot2Lines(packetOutcomePlot, "plots/numberForwardedNumberBadCrc" + os.str() + ".gnu", numberForwardedDataSet2, numberIPDroppedDataSet);
        writePlot(numberForwardedPlot, "plots/numberForwarded" + os.str() + ".gnu", numberForwardedDataSet);
        writePlot(numberIPDroppedPlot, "plots/numberIPDropped" + os.str() + ".gnu", numberIPDroppedDataSet);
        writePlot(numberCollidedPlot, "plots/numberCollided" + os.str() + ".gnu", numberCollidedDataSet);
        writePlot(numberRxfifoFlushesPlot, "plots/numberRxfifoFlushes" + os.str() + ".gnu", numberRxfifoFlushesDataSet);
        writePlot(numberBadCrcPlot, "plots/numberBadCrc" + os.str() + ".gnu", numberBadCrcDataSet);
        writePlot(intraOsDelayPlot, "plots/intraOsDelay" + os.str() + ".gnu", intraOsDelayDataSet);

        /*if (i == 40)
          i = 8;
        if (i == 88)
          i = 48;*/
    }

    numberForwardedFile.close();
#endif

    return 0;
}
