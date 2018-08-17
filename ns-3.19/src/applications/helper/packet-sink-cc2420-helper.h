#ifndef PACKET_SINK_CC2420_HELPER_H
#define PACKET_SINK_CC2420_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"

namespace ns3 {

/**
 * \brief A helper to make it easier to instantiate an ns3::PacketSinkCC2420Application
 * on a set of nodes.
 */
class PacketSinkCC2420Helper
{
public:
  /**
   * Create a PacketSinkCC2420Helper to make it easier to work with PacketSinkCC2420Applications
   */
  PacketSinkCC2420Helper ();

  /**
   * Helper function used to set the underlying application attributes.
   *
   * \param name the name of the application attribute to set
   * \param value the value of the application attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * Install an ns3::PacketSinkCC2420Application on each node of the input container
   * configured with all the attributes set with SetAttribute.
   *
   * \param c NodeContainer of the set of nodes on which a PacketSinkCC2420Application
   * will be installed.
   */
  ApplicationContainer Install (NodeContainer c) const;

  /**
   * Install an ns3::PacketSinkCC2420Application on each node of the input container
   * configured with all the attributes set with SetAttribute.
   *
   * \param node The node on which a PacketSinkCC2420Application will be installed.
   */
  ApplicationContainer Install (Ptr<Node> node) const;

  /**
   * Install an ns3::PacketSinkCC2420Application on each node of the input container
   * configured with all the attributes set with SetAttribute.
   *
   * \param nodeName The name of the node on which a PacketSinkCC2420Application will be installed.
   */
  ApplicationContainer Install (std::string nodeName) const;

private:
  /**
   * \internal
   */
  Ptr<Application> InstallPriv (Ptr<Node> node) const;
  ObjectFactory m_factory;
};

} // namespace ns3

#endif /* PACKET_SINK_CC2420_HELPER_H */
