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

#include "ns3/internet-module.h"
#include "ns3/cc2420-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

#include <sstream>

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TelosB");

namespace ns3 {
    // For debug
    extern bool debugOn;
}

static uint32_t seed = 3;
static double duration = 10;
static int pps = 100;
static bool print = 0;
static int packet_size = 125;
static std::string deviceFile = "device-files/telosb-min.device";  // Required if we use gdb
static std::string trace_fn = "trace-inputs/packets-received.txt";
static std::string kbps = "65kbps";


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
                      cpu,
                      ee->m_serviceMap[interruptId],
                      dummyProgramLoc);

}

class CC2420 {
public:
    bool rxfifo_overflow = false;
    int bytes_in_rxfifo = 0;
    DataRate datarate;
    int nr_send_recv = 0;
    bool collision = false;

    CC2420() {
      datarate = DataRate("250kbps");
    }
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

    Mote() {
      static int cnt;
      id = cnt++;
    }

public:
    Ptr<Node> GetNode() {
      return node;
    }
};

class TelosB : public Mote {
private:
    // Components of the mote
    CC2420 radio;
    int number_forwarded_and_acked = 0;
    int packets_in_send_queue = 0;
    bool receivingPacket = false;
    std::vector<Ptr<Packet> > receive_queue;

    Address src;
    Address dst;
    Ptr<CC2420InterfaceNetDevice> netDevice;

public:
    TelosB(Ptr<Node> node, Address src, Ptr<CC2420InterfaceNetDevice> netDevice) : Mote() {
        TelosB::node = node;
        TelosB::number_forwarded_and_acked = 0;
        TelosB::packets_in_send_queue = 0;
        TelosB::receivingPacket = false;
        TelosB::src = src;
        TelosB::netDevice = netDevice;
    }

    TelosB(Ptr<Node> node, Address src, Address dst, Ptr<CC2420InterfaceNetDevice> netDevice) : Mote() {
        TelosB::node = node;
        TelosB::number_forwarded_and_acked = 0;
        TelosB::packets_in_send_queue = 0;
        TelosB::receivingPacket = false;
        TelosB::src = src;
        TelosB::dst = dst;
        TelosB::netDevice = netDevice;
    }

    TelosB(Ptr<Node> node) : Mote() {
        TelosB::node = node;
        TelosB::number_forwarded_and_acked = 0;
        TelosB::packets_in_send_queue = 0;
        TelosB::receivingPacket = false;
    }

    int cur_nr_packets_processing = 0;

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
            if (ns3::debugOn)
                std::cout << "Dropping packet " << packet->m_executionInfo.seqNr << " due to RXFIFO overflow" << std::endl;
            return;
        }

        if (++cur_nr_packets_processing == 1) {
            ScheduleInterrupt (node, packet, "HIRQ-11", Seconds(0));
        }

        radio.bytes_in_rxfifo += packet->GetSize (); //+ 36;
        if (ns3::debugOn) {
            std::cout << "radio.bytes_in_rxfifo: " << radio.bytes_in_rxfifo << std::endl;
            std::cout << "packet->GetSize(): " << packet->GetSize () << std::endl;
        }
       if (radio.bytes_in_rxfifo > 128) {
            radio.bytes_in_rxfifo -= packet->GetSize (); //+ 36;
            if (ns3::debugOn)
                std::cout<< id << " RXFIFO overflow" << std::endl;
            packet->collided = true;
            // RemoveAtEnd removes the number of bytes from the received packet that were not received due to overflow.
            packet->RemoveAtEnd(radio.bytes_in_rxfifo - 128);
            radio.bytes_in_rxfifo = 128;
            radio.rxfifo_overflow = true;
        }

        if (receivingPacket) {
            // Here, we should enqueue the packet into a spi_busy queue that handles both read_done_length and senddone_task requests.
            if (ns3::debugOn)
                std::cout << "Adding packet " << packet->m_executionInfo.seqNr << " to receive_queue, length: " << receive_queue.size()+1 << std::endl;
            receive_queue.push_back(packet);
            return;
        }

        if (ns3::debugOn) {
            std::cout<< Simulator::Now() << " " << id << ": CC2420ReceivePacket, next step readDoneLength, radio busy " << packet->m_executionInfo.seqNr << std::endl;
        }

        execenv->Proceed(packet, "readdonelength", &TelosB::read_done_length, this, packet);

        ScheduleInterrupt(node, packet, "HIRQ-1", NanoSeconds(10));
        execenv->queues["h1-h2"]->Enqueue(packet);
        receivingPacket = true;
    }

    void read_done_length(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;
        if (ns3::debugOn)
            std::cout<< Simulator::Now() << " " << id << ": readDone_length, next step readDoneFcf " << packet->m_executionInfo.seqNr << std::endl;
        execenv->Proceed(packet, "readdonefcf", &TelosB::readDone_fcf, this, packet);
        execenv->queues["h2-h3"]->Enqueue(packet);
    }

    void readDone_fcf(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (ns3::debugOn)
            std::cout<< Simulator::Now() << " " << id << ": readDone_fcf, next step readDonePayload " << packet->m_executionInfo.seqNr << std::endl;
        execenv->Proceed(packet, "readdonepayload", &TelosB::readDone_payload, this, packet);
        execenv->queues["h3-h4"]->Enqueue(packet);
        execenv->queues["h3-bytes"]->Enqueue(packet);
    }

    void readDone_payload(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        radio.bytes_in_rxfifo -= packet->GetSize ();
        if (radio.rxfifo_overflow && radio.bytes_in_rxfifo <= 0) {
            if (ns3::debugOn)
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
            nr_packets_dropped_bad_crc++;
            if (ns3::debugOn)
                std::cout<< Simulator::Now() << " " << id << ": readDone_payload, collision caused packet CRC check to fail, dropping it " << packet->m_executionInfo.seqNr << std::endl;
            if (!receive_queue.empty()) {
              Ptr<Packet> nextPacket = receive_queue.front();
              receive_queue.erase(receive_queue.begin());
              execenv->Proceed(nextPacket, "readdonelength", &TelosB::read_done_length, this, nextPacket);
              ScheduleInterrupt(node, nextPacket, "HIRQ-1", Seconds(0));
              execenv->queues["h1-h2"]->Enqueue(nextPacket);
            } else {
                receivingPacket = false;
                if (radio.rxfifo_overflow && radio.bytes_in_rxfifo > 0) {
                    if (ns3::debugOn)
                        std::cout<< "RXFIFO gets flushed" << std::endl;
                    radio.rxfifo_overflow = false;
                    radio.bytes_in_rxfifo = 0;
                    nr_rxfifo_flushes++;
                }
            }
        } else {
            if (ns3::debugOn)
                std::cout << "readDone_payload seqno: " << packet->m_executionInfo.seqNr << std::endl;
            execenv->Proceed(packet, "receivedone", &TelosB::receiveDone_task, this, packet);
            execenv->queues["h4-rcvd"]->Enqueue(packet);
        }

        if (ns3::debugOn)
            std::cout << Simulator::Now() << " " << id << ": readDone_payload " << packet->m_executionInfo.seqNr << ", receivingPacket: " << receivingPacket << ", packet collided: " << packet->collided << std::endl;
    }

    bool jitterExperiment = false;

    void receiveDone_task(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;
        if (ns3::debugOn)
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
                    if (ns3::debugOn)
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
            if (ns3::debugOn)
                std::cout<< Simulator::Now() << " " << id << ": receiveDone " << packet->m_executionInfo.seqNr << std::endl;
            execenv->queues["ip-bytes"]->Enqueue(packet);
        } else {
            cur_nr_packets_processing--;
            if (cur_nr_packets_processing == 0) {
                ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
            }
            ++nr_packets_dropped_ip_layer;
            ScheduleInterrupt(node, packet, "HIRQ-17", MicroSeconds(1));
            if (ns3::debugOn)
                std::cout<< Simulator::Now() << " " << id << ": receiveDone_task, queue full, dropping packet " << packet->m_executionInfo.seqNr << std::endl;
        }

        if (!receive_queue.empty()) {
            Ptr<Packet> nextPacket = receive_queue.front();
            receive_queue.erase(receive_queue.begin());
            execenv->Proceed(nextPacket, "readdonelength", &TelosB::read_done_length, this, nextPacket);
            ScheduleInterrupt(node, nextPacket, "HIRQ-1", Seconds(0));
            execenv->queues["h1-h2"]->Enqueue(nextPacket);
        } else {
            receivingPacket = false;
            if (radio.rxfifo_overflow && radio.bytes_in_rxfifo > 0) {
                if (ns3::debugOn)
                    std::cout<< "RXFIFO gets flushed" << std::endl;
                radio.rxfifo_overflow = false;
                radio.bytes_in_rxfifo = 0;
                nr_rxfifo_flushes++;
            }
        }
    }

    bool ip_radioBusy = false;

    void sendTask() {
        // TODO: Reschedule this event if a packet can get read into memory. It seems that events run in parallell when we don't want them to.
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        if (execenv->queues["send-queue"]->IsEmpty()) {
            if (ns3::debugOn)
                std::cout<< "There are no packets in the send queue, returning from sendTask" << std::endl;
            return;
        }


        if (ip_radioBusy) {
            // finishedTransmitting() calls this function again when ip_radioBusy is set to false.
            if (ns3::debugOn)
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
        if (ns3::debugOn)
            std::cout<< Simulator::Now() << " " << id << ": sendTask " << packet->m_executionInfo.seqNr << std::endl;

        ip_radioBusy = true;
    }

    void sendViaCC2420(Ptr<Packet> packet) {
        uint8_t nullBuffer[packet->GetSize()];
        for(uint32_t i=0; i<packet->GetSize(); i++) nullBuffer[i] = 0;

        // send with CCA
        Ptr<CC2420Send> msg = CreateObject<CC2420Send>(nullBuffer, packet->GetSize(), true);

        netDevice->descendingSignal(msg);
    }

    bool ccaOn = true;
    bool fakeSending = false;
    int number_forwarded = 0;
    // Called when done writing packet into TXFIFO, and radio is ready to send
    void sendDoneTask(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (!packet->attemptedSent) {
            //packet->AddPaddingAtEnd (36);
            packet->attemptedSent = true;
            packet->m_executionInfo.timestamps.push_back(Simulator::Now());
            int intra_os_delay = packet->m_executionInfo.timestamps[2].GetMicroSeconds() - packet->m_executionInfo.timestamps[1].GetMicroSeconds();
            time_received_packets.push_back (packet->m_executionInfo.timestamps[1].GetMicroSeconds());
            forwarded_packets_seqnos.push_back (packet->m_executionInfo.seqNr);
            all_intra_os_delays.push_back(intra_os_delay);
            total_intra_os_delay += intra_os_delay;
            if (ns3::debugOn) {
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

        if (radio.nr_send_recv > 0) {
            if (ccaOn) {  // 2500 comes from traces
                Simulator::Schedule(MicroSeconds(2400 + rand() % 200), &TelosB::sendDoneTask, this, packet);
                return;
            }
            radio.collision = true;
            if (ns3::debugOn)
                std::cout << "Forwarding packet " << packet->m_executionInfo.seqNr << " causes collision" << std::endl;
        }

        Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize ()+36 + 5) + MicroSeconds (192), &TelosB::finishedTransmitting, this, packet);
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
        if (ns3::debugOn)
            std::cout << Simulator::Now() << " " << id << ": finishedTransmitting: DELTA: " << packet->m_executionInfo.timestamps[3] - packet->m_executionInfo.timestamps[0] << ", UDP payload size: " << packet->GetSize () << ", seq no: " << packet->m_executionInfo.seqNr << std::endl;
        --packets_in_send_queue;
        --radio.nr_send_recv;
        if (--cur_nr_packets_processing == 0) {
            ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
        }

        if (radio.collision) {
            if (ns3::debugOn)
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
        if (ns3::debugOn)
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
            if (ns3::debugOn)
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
            if (ns3::debugOn)
                std::cout<< "SendPacket, failed to send because radio's RXFIFO is overflowed" << std::endl;
        }
    }

    bool use_device_model = true;
    int seqNr = 0;
    bool HandleRead (Ptr<CC2420Message> msg)
    {
      //NS_LOG_INFO ("Received message from CC2420InterfaceNetDevice");

      // What does it mean that it has not been received correctly? Bad CRC?
      if(msg==NULL){
        std::cout << "Message not correctly received!" << std::endl;
        return false;
      }


      Ptr<CC2420Recv> recvMsg = DynamicCast<CC2420Recv>(msg);
      if(recvMsg){
          //std::cout << "THIS is the place where the device model gets involved and forwards the packet to mote 3" << std::endl;
          //std::cout << "At time " << Simulator::Now ().GetSeconds ()
          //        << "s mote " << GetId() << " received " << recvMsg->getSize()
          //        << " bytes with CRC=" << (recvMsg->getCRC()?"true":"false")
          //        << " and RSSI=" << recvMsg->getRSSI() << " bytes" << std::endl;

          Ptr<Packet> packet = Create<Packet>(packet_size);
          nr_packets_total++;
          packet->m_executionInfo.timestamps.push_back (Simulator::Now());
          packet->src = src;
          packet->dst = dst;
          packet->m_executionInfo.seqNr = seqNr++;
          if (use_device_model)
              ReceivePacket (packet);
          else
              sendViaCC2420 (packet);
          return true;

      } else {
          Ptr<CC2420Cca> ccaMsg = DynamicCast<CC2420Cca>(msg);
          if(ccaMsg){
              //std::cout << "At time " << Simulator::Now ().GetSeconds ()
              //        << "s mote " << GetId() << " received CC2420Cca message with channel free = "
              //        << (ccaMsg->getCcaValue()?"true":"false") << std::endl;
              return true;

          } else {
              Ptr<CC2420Sending> sendingMsg = DynamicCast<CC2420Sending>(msg);
              if(sendingMsg){
                  //std::cout << "At time " << Simulator::Now ().GetSeconds ()
                  //        << "s mote " << GetId() << " received CC2420Sending message with can send = "
                  //        << (sendingMsg->getSending()?"true":"false") << std::endl;
                  if (!sendingMsg->getSending ()) {
                      // This means we failed to send packet because channel is busy
                      //std::cout << "recvMsg->getSize (): " << recvMsg->getSize () << std::endl;
                      Ptr<Packet> packet = Create<Packet>(packet_size);
                      packet->attemptedSent = true;
                      Simulator::Schedule(Seconds(0.0025), &TelosB::sendDoneTask, this, packet);
                  }
                  return true;

              } else {
                  Ptr<CC2420SendFinished> sfMsg = DynamicCast<CC2420SendFinished>(msg);
                  if(sfMsg){
                      //std::cout << "At time " << Simulator::Now ().GetSeconds ()
                      //        << "s mote " << GetId() << " received CC2420SendFinished message" << std::endl;

                      finishedTransmitting (Create<Packet>(packet_size));
                      return true;

                  } else {
                      Ptr<CC2420StatusResp> respMsg = DynamicCast<CC2420StatusResp>(msg);
                      if(respMsg){
                          /*std::cout << "At time " << Simulator::Now ().GetSeconds ()
                                  << "s mote " << GetId() << " received CC2420StatusResp message with values"
                                  << " CCA mode=" << (int) respMsg->getCcaMode()
                                  << ", CCA hysteresis=" << (int) respMsg->getCcaHysteresis()
                                  << ", CCA threshold=" << (int) respMsg->getCcaThreshold()
                                  << ", long TX turnaround=" << (respMsg->getTxTurnaround()?"true":"false")
                                  << ", automatic CRC=" << (respMsg->getAutoCrc()?"true":"false")
                                  << ", preamble length=" << (int) respMsg->getPreambleLength()
                                  << ", sync word=0x" << std::hex << (int) respMsg->getSyncWord() << std::dec
                                  << ", channel=" << (int) respMsg->getChannel()
                                  << ", power=" << (int) respMsg->getPower() << std::endl;*/
                          return true;
                      } else {
                          //unknown message or NULL-Pointer
                          std::cout << "CC2420Message is of an unknown type!" << std::endl;
                          return false;
                      } //unknown
                  } //status response
              } // send finished
          } // sending
      } // receive

      return false; // something went wrong
    }
};

class ProtocolStack {
public:
	void GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TelosB *mote1, TelosB *mote2, TelosB *mote3);
	void GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TelosB *mote1, TelosB *mote2, TelosB *mote3);
	void GeneratePacket(uint32_t pktSize, uint32_t curSeqNr, TelosB *mote1, TelosB *mote2, TelosB *mote3);
};

Gnuplot *ppsPlot = NULL;
Gnuplot *delayPlot = NULL;
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
}

void writePlot2Lines(Gnuplot* plot, std::string filename, Gnuplot2dDataset* dataSet1, Gnuplot2dDataset* dataSet2) {
    plot->AddDataset(*dataSet1);
    plot->AddDataset(*dataSet2);
    std::ofstream plotFile(filename.c_str());
    plot->GenerateOutput(plotFile);
    plotFile.close();
}

// GeneratePacket creates a packet and passes it on to the NIC
void ProtocolStack::GeneratePacket(uint32_t pktSize, uint32_t curSeqNr, TelosB *mote1, TelosB *mote2, TelosB *mote3) {
    Ptr<Packet> toSend = Create<Packet>(pktSize);
        toSend->m_executionInfo.seqNr = curSeqNr;
        toSend->m_executionInfo.executedByExecEnv = false;

    if (ns3::debugOn)
        std::cout<< "Generating packet " << curSeqNr << std::endl;

    mote1->SendPacket(toSend, mote2, mote3);
}

// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate.
void ProtocolStack::GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TelosB *mote1, TelosB *mote2, TelosB *mote3) {
    static int curSeqNr = 0;

    GeneratePacket(pktSize, curSeqNr++, mote1, mote2, mote3);
        if (Simulator::Now().GetSeconds() + (1.0 / (double) pps) < duration - 0.02)
                Simulator::Schedule(Seconds(1.0 / (double) pps) + MicroSeconds(rand() % 100),
                &ProtocolStack::GenerateTraffic, this, n, /*rand()%(80 + 1)*/pktSize, mote1, mote2, mote3);
}


// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate.
void ProtocolStack::GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TelosB *mote1, TelosB *mote2, TelosB *mote3) {
    Simulator::Schedule(time,
      &ProtocolStack::GenerateTraffic, this, n, /*rand()%(80 + 1)*/pktSize, mote1, mote2, mote3);
}

int main(int argc, char *argv[])
{
    // Debugging and tracing
    ns3::debugOn = true;

    // Fetch from command line
    CommandLine cmd;
    cmd.AddValue("seed", "seed for the random generator", seed);
    cmd.AddValue("duration", "The number of seconds the simulation should run", duration);
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
#define ALL_CONTEXTS 0
#define CC2420_MODEL 1
#if CC2420_MODEL
    CC2420Helper cc2420;

    NodeContainer nodes;
    nodes.Create(3);

    NetDeviceContainer devices;
    devices = cc2420.Install(nodes, true); // regular CC2420NetDevice

    InternetStackHelper stack;
    stack.Install(nodes);

    MobilityHelper mobility;

    // The way we want to configure this: mote 1 receives the packet from mote 2, but mote 3 does not receive it. Mote 3 receives the packet from mote 2.
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (30.0),
                                   "DeltaY", DoubleValue (10.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    //Ipv4InterfaceAddress src = interfaces.Get(0).first->GetAddress (1, 0);  // 10.0.0.1
    //Ipv4InterfaceAddress dst = interfaces.Get(2).first->GetAddress (1, 0);  // 10.0.0.3
    Ptr<CC2420InterfaceNetDevice> netDevice1 = nodes.Get(0)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
    Ptr<CC2420InterfaceNetDevice> netDevice2 = nodes.Get(1)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
    Ptr<CC2420InterfaceNetDevice> netDevice3 = nodes.Get(2)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
    TelosB *mote1 = new TelosB(nodes.Get(0), InetSocketAddress(interfaces.GetAddress(0), 9), netDevice1);
    //mote1->Send(src, dst, packet);  // Use case
    TelosB *mote2 = new TelosB(nodes.Get(1), InetSocketAddress(interfaces.GetAddress(1), 9), InetSocketAddress(interfaces.GetAddress(2), 9), netDevice2);
    TelosB *mote3 = new TelosB(nodes.Get(2), InetSocketAddress(interfaces.GetAddress(2), 9), netDevice3);
    print = false;

    // Send (Ptr<Packet> packet, bool checkCCA, const Address& dest, uint16_t protocolNumber);
    // Mote 1 sends packet to mote 3
    //DynamicCast<CC2420InterfaceNetDevice>(devices.Get (0))->Send (Create<Packet>(124), InetSocketAddress(interfaces.GetAddress(1), 9), 0);

    //uint32_t m_pktSize = 88;
    //uint8_t nullBuffer[m_pktSize];
    //for(uint32_t i=0; i<m_pktSize; i++) nullBuffer[i] = 0;

    // send with CCA
    //Ptr<CC2420Send> msg = CreateObject<CC2420Send>(nullBuffer, m_pktSize, true);

    //m_totBytes += m_pktSize;

    //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
    //        << "s on-off-cc2420 application sent " <<  m_pktSize << " bytes"
    //        << " total Tx " << m_totBytes << " bytes");
    //uint8_t channelNo = 12; // channel number is changed from default value 11 to 12
    //uint8_t power = 31; // power remains on default value of 31
    //Ptr<CC2420Message> msg2 = CreateObject<CC2420Setup>(channelNo, power);
    //construct Config message (default power, changed channel)
    //DynamicCast<CC2420InterfaceNetDevice>(devices.Get(0))->descendingSignal(msg2);

    // request current status
    //DynamicCast<CC2420InterfaceNetDevice>(devices.Get(0))->descendingSignal(CreateObject<CC2420StatusReq>());
    //DynamicCast<CC2420InterfaceNetDevice>(devices.Get(0))->descendingSignal(msg);
    /*DynamicCast<CC2420InterfaceNetDevice>(devices.Get (0))->Send (Create<Packet>(124), InetSocketAddress(interfaces.GetAddress(2), 9), 0);
    DynamicCast<CC2420InterfaceNetDevice>(devices.Get (0))->Send (Create<Packet>(124), InetSocketAddress(interfaces.GetAddress(2), 9), 0);
    DynamicCast<CC2420InterfaceNetDevice>(devices.Get (0))->Send (Create<Packet>(124), InetSocketAddress(interfaces.GetAddress(2), 9), 0);
    DynamicCast<CC2420InterfaceNetDevice>(devices.Get (0))->Send (Create<Packet>(124), InetSocketAddress(interfaces.GetAddress(2), 9), 0);
    DynamicCast<CC2420InterfaceNetDevice>(devices.Get (0))->Send (Create<Packet>(124), InetSocketAddress(interfaces.GetAddress(2), 9), 0);*/
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    eeh->Install(deviceFile, mote2->GetNode());

    ScheduleInterrupt (mote2->GetNode(), Create<Packet>(0), "HIRQ-12", Seconds(0));

    // send packets to PacketSink (installed on node 1)
    OnOffCC2420Helper onoff;
    // 80kbps ist die "Grenze", bei der bei einer Paketgröße von 20 Bytes gerade noch alle Pakete ankommen

    // first simulation (DataRate and PacketSize ok)
    onoff.SetAttribute("DataRate", StringValue(kbps));
    onoff.SetAttribute("PacketSize", StringValue(SSTR( packet_size ))); //default is 512, which is too much for CC2420

    // second simulation (DataRate too high, PacketSize ok)
    //onoff.SetAttribute("DataRate", StringValue("90kbps"));
    //onoff.SetAttribute("PacketSize", StringValue("20"));

    // third simulation (DataRate ok, PacketSize too big)
    //onoff.SetAttribute("DataRate", StringValue("70kbps"));
    //onoff.SetAttribute("PacketSize", StringValue("150"));

    ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    //clientApps.Start(Seconds(1.0));
    //clientApps.Stop(Seconds(5.0));

    netDevice2->SetMessageCallback(MakeCallback(&TelosB::HandleRead, mote2));

    uint8_t channelNo = 12; // channel number is changed from default value 11 to 12
    uint8_t power = 31; // power remains on default value of 31
    Ptr<CC2420Message> msg = CreateObject<CC2420Setup>(channelNo, power);
    //construct Config message (default power, changed channel)
    netDevice2->descendingSignal(msg);

    // request current status
    netDevice2->descendingSignal(CreateObject<CC2420StatusReq>());

    PacketSinkCC2420Helper pktSink;

    ApplicationContainer serverApps = pktSink.Install(nodes.Get(2));
    //serverApps.Start(Seconds(1.0));
    //serverApps.Stop(Seconds(5.0));

    duration = 8.01;
    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "UDP payload: " << packet_size << ", pps: " << pps << ", RXFIFO flushes: " << nr_rxfifo_flushes << ", bad CRC: " << nr_packets_dropped_bad_crc << ", radio collision: " << nr_packets_collision_missed << ", ip layer drop: " << nr_packets_dropped_ip_layer << ", successfully forwarded: " << nr_packets_forwarded << " / " << nr_packets_total << " = " << (nr_packets_forwarded/(float)nr_packets_total)*100 << "% in " << (duration/2 + (int)duration % 2) << " seconds, actual pps=" << (nr_packets_forwarded/(duration/2 + (int)duration % 2)) << std::endl;

#elif READ_TRACES
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
        //                    &ProtocolStack::GeneratePacket, protocolStack, packet_size, curSeqNr++, mote1, mote2, mote3);
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

    //Ptr<ExecEnv> ee1 = c.Get(0)->GetObject<ExecEnv>();
    //Ptr<ExecEnv> ee2 = c.Get(1)->GetObject<ExecEnv>();
    //Ptr<ExecEnv> ee3 = c.Get(2)->GetObject<ExecEnv>();
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
    int numberMotes = 20000;
    pps = 1;
    duration = 0.5;
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
    std::cout << "Before" << std::endl;
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
            all_intra_os_delays.empty();

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

        if (i == 40)
          i = 8;
        if (i == 88)
          i = 48;
    }

    numberForwardedFile.close();
#endif

    return 0;
}
