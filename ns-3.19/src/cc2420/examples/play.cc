/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/packet.h"
#include "ns3/cc2420-module.h"

#include <iostream>

NS_LOG_COMPONENT_DEFINE ("ScratchSimulator");

using namespace ns3;



class CC2420PreambleTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

  // these are our accessors to our tag structure
  void SetPreambleLength (uint8_t preambleLength);
  uint8_t GetPreambleLength (void) const;
private:
  uint8_t m_premableLength;
};

TypeId
CC2420PreambleTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CC2420PreambleTag")
    .SetParent<Tag> ()
    .AddConstructor<CC2420PreambleTag> ()
    .AddAttribute ("PreambleLength",
                   "The preamble length",
                   UintegerValue (2),
                   MakeUintegerAccessor (&CC2420PreambleTag::GetPreambleLength),
                   MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}
TypeId
CC2420PreambleTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t
CC2420PreambleTag::GetSerializedSize (void) const
{
  return 1;
}
void
CC2420PreambleTag::Serialize (TagBuffer i) const
{
  i.WriteU8 (m_premableLength);
}
void
CC2420PreambleTag::Deserialize (TagBuffer i)
{
  m_premableLength = i.ReadU8 ();
}
void
CC2420PreambleTag::Print (std::ostream &os) const
{
  os << "Preamble_Length=" << (uint32_t)m_premableLength;
}
void
CC2420PreambleTag::SetPreambleLength (uint8_t preambleLength)
{
  m_premableLength = preambleLength;
}
uint8_t
CC2420PreambleTag::GetPreambleLength (void) const
{
  return m_premableLength;
}

int 
main (int argc, char *argv[])
{
  const uint8_t packet_length = 127;
  const uint16_t syncWord = 0xA70F;
  const uint8_t preamble_length = 15;

  Packet::EnablePrinting();
  Ptr<Packet> packet = Create<Packet>(packet_length);

  CC2420Header hdr;
  hdr.SetData(preamble_length,syncWord,packet->GetSize());
  NS_LOG_UNCOND ("Header " << hdr);

//  CC2420PreambleTag tag;
//  tag.SetPreambleLength(preamble_length);
//  std::cout << "Tag: ";
//  tag.Print (std::cout);
//  std::cout << std::endl;

  packet->AddHeader(hdr);
//  packet->AddPacketTag(tag);

  NS_LOG_UNCOND ("Packet: " << *packet);

//  //set dummy value
//  CC2420PreambleTag newtag;

//  packet->RemovePacketTag(newtag);
//  std::cout << "newTag: ";
//  newtag.Print (std::cout);
//  std::cout << std::endl;


  CC2420Header newhdr;
  packet->RemoveHeader(newhdr);
  NS_LOG_UNCOND ("Header " << newhdr);
  NS_LOG_UNCOND ("SyncWords: 0x" << std::hex << newhdr.GetSyncWord() << " - 0x" << syncWord);
  NS_LOG_UNCOND ("Same SyncWord: " << std::boolalpha << newhdr.CompareSyncWord(syncWord));

  uint8_t ack_example[3];
  ack_example[0] = 0b01000000;
  ack_example[1] = 0b00000000;
  ack_example[2] = 0b01010110;

  Ptr<Packet> ack_packet = Create<Packet>(ack_example, 3);
  CC2420Trailer tail;
  tail.CalcFcs(ack_packet);
  NS_LOG_UNCOND ("Trailer " << tail);


  Mac48Address a1 = Mac48Address::Allocate();
  Mac48Address a2 = Mac48Address::Allocate();

  AddressTag atag;
  atag.SetData(a1, a2, 17);
  NS_LOG_UNCOND("Tag: "<< atag);
  packet->AddPacketTag(atag);
  NS_LOG_UNCOND("Packet: "<< *packet);
  AddressTag newtag;
  packet->RemovePacketTag(newtag);
  NS_LOG_UNCOND("Tag: "<< newtag);


  NS_LOG_UNCOND("Packet: "<< *packet);
  packet->AddTrailer(tail);
  NS_LOG_UNCOND ("Packet="<<*packet<<" GetSize="<<packet->GetSize()<<" GetSerializedSize="<< packet->GetSerializedSize());
  CC2420Trailer newtail;
  packet->RemoveTrailer(newtail);
  NS_LOG_UNCOND ("Trailer " << newtail);


//  NS_LOG_UNCOND ("EMPTY-EMPTY-EMPTY-EMPTY-EMPTY-EMPTY-EMPTY-EMPTY-EMPTY-EMPTY");
//  Ptr<Packet> newpacket = Create<Packet>(ack_example, 3);
//  CC2420Header emptyhdr;
//  NS_LOG_UNCOND ("Header " << emptyhdr);
//  NS_LOG_UNCOND ("Packet: " << *newpacket);
//  newpacket->RemoveHeader(emptyhdr);
//  NS_LOG_UNCOND ("Header " << emptyhdr);
//  NS_LOG_UNCOND ("Packet: " << *newpacket);
}
