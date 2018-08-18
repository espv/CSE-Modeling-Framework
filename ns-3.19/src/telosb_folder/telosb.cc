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

#include "telosb.h"

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TelosB");

namespace ns3 {
    // For debug
    extern bool debugOn;
}

static ProgramLocation *dummyProgramLoc;

static ProtocolStack ps;
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


TelosB::TelosB(Ptr<Node> node, Address src, Ptr<CC2420InterfaceNetDevice> netDevice) : Mote() {
        TelosB::node = node;
        TelosB::number_forwarded_and_acked = 0;
        TelosB::packets_in_send_queue = 0;
        TelosB::receivingPacket = false;
        TelosB::src = src;
        TelosB::netDevice = netDevice;
    }

TelosB::TelosB(Ptr<Node> node, Address src, Address dst, Ptr<CC2420InterfaceNetDevice> netDevice) : Mote() {
        TelosB::node = node;
        TelosB::number_forwarded_and_acked = 0;
        TelosB::packets_in_send_queue = 0;
        TelosB::receivingPacket = false;
        TelosB::src = src;
        TelosB::dst = dst;
        TelosB::netDevice = netDevice;
    }

TelosB::TelosB(Ptr<Node> node) : Mote() {
        TelosB::node = node;
        TelosB::number_forwarded_and_acked = 0;
        TelosB::packets_in_send_queue = 0;
        TelosB::receivingPacket = false;
    }

    // Models the radio's behavior before the packets are processed by the microcontroller.
    void TelosB::ReceivePacket(Ptr<Packet> packet) {
        ps.firstNodeSendingtal = false;
        --radio.nr_send_recv;
        packet->m_executionInfo.timestamps.push_back(Simulator::Now());
        packet->collided = radio.collision;
        if (radio.collision && radio.nr_send_recv == 0)
            radio.collision = false;

        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (radio.rxfifo_overflow) {
            if (ns3::debugOn)
                NS_LOG_INFO ("Dropping packet " << packet->m_executionInfo.seqNr << " due to RXFIFO overflow");
            return;
        }

        if (++cur_nr_packets_processing == 1) {
            ScheduleInterrupt (node, packet, "HIRQ-11", Seconds(0));
        }

        radio.bytes_in_rxfifo += packet->GetSize ();
        if (ns3::debugOn) {
            NS_LOG_INFO ("radio.bytes_in_rxfifo: " << radio.bytes_in_rxfifo);
            NS_LOG_INFO ("packet->GetSize(): " << packet->GetSize ());
        }
       if (radio.bytes_in_rxfifo > 128) {
            radio.bytes_in_rxfifo -= packet->GetSize (); //+ 36;
            if (ns3::debugOn)
                NS_LOG_INFO (id << " RXFIFO overflow");
            packet->collided = true;
            // RemoveAtEnd removes the number of bytes from the received packet that were not received due to overflow.
            packet->RemoveAtEnd(radio.bytes_in_rxfifo - 128);
            radio.bytes_in_rxfifo = 128;
            radio.rxfifo_overflow = true;
        }

        if (receivingPacket) {
            if (ns3::debugOn)
                NS_LOG_INFO ("Adding packet " << packet->m_executionInfo.seqNr << " to receive_queue, length: " << receive_queue.size()+1);
            receive_queue.push_back(packet);
            return;
        }

        if (ns3::debugOn) {
            NS_LOG_INFO (Simulator::Now() << " " << id << ": CC2420ReceivePacket, next step readDoneLength, radio busy " << packet->m_executionInfo.seqNr);
        }

        execenv->Proceed(packet, "readdonelength", &TelosB::read_done_length, this, packet);

        ScheduleInterrupt(node, packet, "HIRQ-1", NanoSeconds(10));
        execenv->queues["h1-h2"]->Enqueue(packet);
        receivingPacket = true;
    }

    void TelosB::read_done_length(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;
        if (ns3::debugOn)
            NS_LOG_INFO (Simulator::Now() << " " << id << ": readDone_length, next step readDoneFcf " << packet->m_executionInfo.seqNr);
        execenv->Proceed(packet, "readdonefcf", &TelosB::readDone_fcf, this, packet);
        execenv->queues["h2-h3"]->Enqueue(packet);
    }

    void TelosB::readDone_fcf(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (ns3::debugOn)
            NS_LOG_INFO (Simulator::Now() << " " << id << ": readDone_fcf, next step readDonePayload " << packet->m_executionInfo.seqNr);
        execenv->Proceed(packet, "readdonepayload", &TelosB::readDone_payload, this, packet);
        execenv->queues["h3-h4"]->Enqueue(packet);
        execenv->queues["h3-bytes"]->Enqueue(packet);
    }

    void TelosB::readDone_payload(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        radio.bytes_in_rxfifo -= packet->GetSize ();
        if (radio.rxfifo_overflow && radio.bytes_in_rxfifo <= 0) {
            if (ns3::debugOn)
                NS_LOG_INFO ("RXFIFO gets flushed");
            radio.rxfifo_overflow = false;
            radio.bytes_in_rxfifo = 0;
            ps.nr_rxfifo_flushes++;
        }

        // Packets received and causing RXFIFO overflow get dropped.
        if (packet->collided) {
            cur_nr_packets_processing--;
            if (cur_nr_packets_processing == 0) {
                ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
            }
            ps.nr_packets_dropped_bad_crc++;
            if (ns3::debugOn)
                NS_LOG_INFO (Simulator::Now() << " " << id << ": readDone_payload, collision caused packet CRC check to fail, dropping it " << packet->m_executionInfo.seqNr);
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
                        NS_LOG_INFO ("RXFIFO gets flushed");
                    radio.rxfifo_overflow = false;
                    radio.bytes_in_rxfifo = 0;
                    ps.nr_rxfifo_flushes++;
                }
            }
        } else {
            if (ns3::debugOn)
                NS_LOG_INFO ("readDone_payload seqno: " << packet->m_executionInfo.seqNr);
            execenv->Proceed(packet, "receivedone", &TelosB::receiveDone_task, this, packet);
            execenv->queues["h4-rcvd"]->Enqueue(packet);
        }

        if (ns3::debugOn)
            NS_LOG_INFO (Simulator::Now() << " " << id << ": readDone_payload " << packet->m_executionInfo.seqNr << ", receivingPacket: " << receivingPacket << ", packet collided: " << packet->collided);
    }

    void TelosB::receiveDone_task(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;
        if (ns3::debugOn)
            NS_LOG_INFO ("packets_in_send_queue: " << packets_in_send_queue);

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
                        NS_LOG_INFO (Simulator::Now() << " " << id << ": receiveDone " << packet->m_executionInfo.seqNr);
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
                NS_LOG_INFO (Simulator::Now() << " " << id << ": receiveDone " << packet->m_executionInfo.seqNr);
            execenv->queues["ip-bytes"]->Enqueue(packet);
        } else {
            cur_nr_packets_processing--;
            if (cur_nr_packets_processing == 0) {
                ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
            }
            ++ps.nr_packets_dropped_ip_layer;
            ScheduleInterrupt(node, packet, "HIRQ-17", MicroSeconds(1));
            if (ns3::debugOn)
                NS_LOG_INFO (Simulator::Now() << " " << id << ": receiveDone_task, queue full, dropping packet " << packet->m_executionInfo.seqNr);
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
                    NS_LOG_INFO ("RXFIFO gets flushed");
                radio.rxfifo_overflow = false;
                radio.bytes_in_rxfifo = 0;
                ps.nr_rxfifo_flushes++;
            }
        }
    }

    void TelosB::sendTask() {
        // TODO: Reschedule this event if a packet can get read into memory. It seems that events run in parallell when we don't want them to.
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        if (execenv->queues["send-queue"]->IsEmpty()) {
            if (ns3::debugOn)
                NS_LOG_INFO ("There are no packets in the send queue, returning from sendTask");
            return;
        }


        if (ip_radioBusy) {
            // finishedTransmitting() calls this function again when ip_radioBusy is set to false.
            if (ns3::debugOn)
                NS_LOG_INFO ("ip_radioBusy is true, returning from sendTask");
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
            NS_LOG_INFO (Simulator::Now() << " " << id << ": sendTask " << packet->m_executionInfo.seqNr);

        ip_radioBusy = true;
    }

    void TelosB::sendViaCC2420(Ptr<Packet> packet) {
        uint8_t nullBuffer[packet->GetSize()];
        for(uint32_t i=0; i<packet->GetSize(); i++) nullBuffer[i] = 0;

        // send with CCA
        Ptr<CC2420Send> msg = CreateObject<CC2420Send>(nullBuffer, packet->GetSize(), true);

        netDevice->descendingSignal(msg);
    }

    // Called when done writing packet into TXFIFO, and radio is ready to send
    void TelosB::sendDoneTask(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        packet->m_executionInfo.executedByExecEnv = false;

        if (!packet->attemptedSent) {
            //packet->AddPaddingAtEnd (36);
            packet->attemptedSent = true;
            packet->m_executionInfo.timestamps.push_back(Simulator::Now());
            int intra_os_delay = packet->m_executionInfo.timestamps[2].GetMicroSeconds() - packet->m_executionInfo.timestamps[1].GetMicroSeconds();
            ps.time_received_packets.push_back (packet->m_executionInfo.timestamps[1].GetMicroSeconds());
            ps.forwarded_packets_seqnos.push_back (packet->m_executionInfo.seqNr);
            ps.all_intra_os_delays.push_back(intra_os_delay);
            ps.total_intra_os_delay += intra_os_delay;
            if (ns3::debugOn) {
                NS_LOG_INFO (Simulator::Now() << " " << id << ": sendDoneTask " << packet->m_executionInfo.seqNr);
                NS_LOG_INFO (id << " sendDoneTask: DELTA: " << intra_os_delay << ", UDP payload size (36+payload bytes): " << packet->GetSize () << ", seq no " << packet->m_executionInfo.seqNr);
                NS_LOG_INFO (Simulator::Now() << " " << id << ": sendDoneTask, number forwarded: " << ++number_forwarded_and_acked << ", seq no " << packet->m_executionInfo.seqNr);
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
                NS_LOG_INFO ("Forwarding packet " << packet->m_executionInfo.seqNr << " causes collision");
        }

        Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize ()+36 + 5) + MicroSeconds (192), &TelosB::finishedTransmitting, this, packet);
        ++radio.nr_send_recv;
    }

    // Radio is finished transmitting packet, and packet can now be removed from the send queue as there is no reason to ever re-transmit it.
    // If acks are enabled, the ack has to be received before that can be done.
    void TelosB::finishedTransmitting(Ptr<Packet> packet) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        ++ps.nr_packets_forwarded;

        // I believe it's here that the packet gets removed from the send queue, but it might be in sendDoneTask
        ip_radioBusy = false;
        packet->m_executionInfo.timestamps.push_back(Simulator::Now());
        if (ns3::debugOn)
            NS_LOG_INFO (Simulator::Now() << " " << id << ": finishedTransmitting: DELTA: " << packet->m_executionInfo.timestamps[3] - packet->m_executionInfo.timestamps[0] << ", UDP payload size: " << packet->GetSize () << ", seq no: " << packet->m_executionInfo.seqNr);
        --packets_in_send_queue;
        --radio.nr_send_recv;
        if (--cur_nr_packets_processing == 0) {
            ScheduleInterrupt (node, packet, "HIRQ-12", Seconds(0));
        }

        if (radio.collision) {
            if (ns3::debugOn)
                NS_LOG_INFO (Simulator::Now() << " finishedTransmitting: Collision occured, destroying packet to be forwarded, radio.nr_send_recv: " << radio.nr_send_recv << ", receivingPacket: " << receivingPacket);
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

    void TelosB::SendPacket(Ptr<Packet> packet, TelosB *to_mote, TelosB *third_mote) {
        Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
        if (ns3::debugOn)
            NS_LOG_INFO (Simulator::Now() << " " << id << ": SendPacket " << packet->m_executionInfo.seqNr);

        // Finish this, also change ReceivePacket to also accept acks
        if (!to_mote->radio.rxfifo_overflow && to_mote->radio.nr_send_recv == 0) {
            if (ps.firstNodeSendingtal) {
                Simulator::Schedule(MicroSeconds(100), &TelosB::SendPacket, this, packet, to_mote, third_mote);
                return;
            }

            ps.firstNodeSendingtal = true;
            ++ps.nr_packets_total;
            ++to_mote->radio.nr_send_recv;
            packet->m_executionInfo.timestamps.push_back(Simulator::Now());
            Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize ()+36 + 5/* 36 is UDP packet, 5 is preamble + SFD*/) + MicroSeconds (192) /* 12 symbol lengths before sending packet, even without CCA. 8 symbol lengths is 128 µs */, &TelosB::ReceivePacket, to_mote, packet);
            if (ns3::debugOn)
                NS_LOG_INFO ("SendPacket, sending packet " << packet->m_executionInfo.seqNr);
        } else if (to_mote->radio.nr_send_recv > 0) {
            if (ccaOn) {
                if (ns3::debugOn)
                  NS_LOG_INFO ("CCA, delaying sending packet");
                Simulator::Schedule(MicroSeconds(2400 + rand() % 200), &TelosB::SendPacket, this, packet, to_mote, third_mote);
                return;
            }
            ++ps.nr_packets_total;
            to_mote->radio.collision = true;
            ++ps.nr_packets_collision_missed;
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
                NS_LOG_INFO ("SendPacket, failed to send because radio's RXFIFO is overflowed");
        }
    }

    bool TelosB::HandleRead (Ptr<CC2420Message> msg)
    {
      //NS_LOG_INFO ("Received message from CC2420InterfaceNetDevice");

      // What does it mean that it has not been received correctly? Bad CRC?
      if(msg==NULL){
        NS_LOG_INFO ("Message not correctly received!");
        return false;
      }


      Ptr<CC2420Recv> recvMsg = DynamicCast<CC2420Recv>(msg);
      if(recvMsg){
          //NS_LOG_INFO ("THIS is the place where the device model gets involved and forwards the packet to mote 3");
          //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
          //        << "s mote " << GetId() << " received " << recvMsg->getSize()
          //        << " bytes with CRC=" << (recvMsg->getCRC()?"true":"false")
          //        << " and RSSI=" << recvMsg->getRSSI() << " bytes");

          Ptr<Packet> packet = Create<Packet>(ps.packet_size);
          ps.nr_packets_total++;
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
              //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
              //        << "s mote " << GetId() << " received CC2420Cca message with channel free = "
              //        << (ccaMsg->getCcaValue()?"true":"false"));
              return true;

          } else {
              Ptr<CC2420Sending> sendingMsg = DynamicCast<CC2420Sending>(msg);
              if(sendingMsg){
                  //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                  //        << "s mote " << GetId() << " received CC2420Sending message with can send = "
                  //        << (sendingMsg->getSending()?"true":"false"));
                  if (!sendingMsg->getSending ()) {
                      // This means we failed to send packet because channel is busy
                      //NS_LOG_INFO ("recvMsg->getSize (): " << recvMsg->getSize ());
                      Ptr<Packet> packet = Create<Packet>(ps.packet_size);
                      packet->attemptedSent = true;
                      Simulator::Schedule(Seconds(0.0025), &TelosB::sendDoneTask, this, packet);
                  }
                  return true;

              } else {
                  Ptr<CC2420SendFinished> sfMsg = DynamicCast<CC2420SendFinished>(msg);
                  if(sfMsg){
                      //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                      //        << "s mote " << GetId() << " received CC2420SendFinished message");

                      finishedTransmitting (Create<Packet>(ps.packet_size));
                      return true;

                  } else {
                      Ptr<CC2420StatusResp> respMsg = DynamicCast<CC2420StatusResp>(msg);
                      if(respMsg){
                          /*NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                                  << "s mote " << GetId() << " received CC2420StatusResp message with values"
                                  << " CCA mode=" << (int) respMsg->getCcaMode()
                                  << ", CCA hysteresis=" << (int) respMsg->getCcaHysteresis()
                                  << ", CCA threshold=" << (int) respMsg->getCcaThreshold()
                                  << ", long TX turnaround=" << (respMsg->getTxTurnaround()?"true":"false")
                                  << ", automatic CRC=" << (respMsg->getAutoCrc()?"true":"false")
                                  << ", preamble length=" << (int) respMsg->getPreambleLength()
                                  << ", sync word=0x" << std::hex << (int) respMsg->getSyncWord() << std::dec
                                  << ", channel=" << (int) respMsg->getChannel()
                                  << ", power=" << (int) respMsg->getPower());*/
                          return true;
                      } else {
                          //unknown message or NULL-Pointer
                          NS_LOG_INFO ("CC2420Message is of an unknown type!");
                          return false;
                      } //unknown
                  } //status response
              } // send finished
          } // sending
      } // receive

      //return false; // something went wrong
    }

// GeneratePacket creates a packet and passes it on to the NIC
void ProtocolStack::GeneratePacket(uint32_t pktSize, uint32_t curSeqNr, TelosB *m1, TelosB *m2, TelosB *m3) {
    Ptr<Packet> toSend = Create<Packet>(pktSize);
        toSend->m_executionInfo.seqNr = curSeqNr;
        toSend->m_executionInfo.executedByExecEnv = false;

    if (ns3::debugOn)
        NS_LOG_INFO ("Generating packet " << curSeqNr);

    m1->SendPacket(toSend, m2, m3);
}

// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate.
void ProtocolStack::GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TelosB *m1, TelosB *m2, TelosB *m3) {
    static int curSeqNr = 0;

    GeneratePacket(pktSize, curSeqNr++, m1, m2, m3);
        if (Simulator::Now().GetSeconds() + (1.0 / (double) ps.pps) < ps.duration - 0.02)
                Simulator::Schedule(Seconds(1.0 / (double) ps.pps) + MicroSeconds(rand() % 100),
                &ProtocolStack::GenerateTraffic, this, n, pktSize, m1, m2, m3);
}


// GenerateTraffic schedules the generation of packets according to the duration
// of the experiment and the specified (static) rate.
void ProtocolStack::GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TelosB *m1, TelosB *m2, TelosB *m3) {
    Simulator::Schedule(time, &ProtocolStack::GenerateTraffic, this, n, pktSize, m1, m2, m3);
}
