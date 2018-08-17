#ifndef ON_OFF_CC2420_HELPER_H
#define ON_OFF_CC2420_HELPER_H

#include <stdint.h>
#include <string>
#include "ns3/object-factory.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/onoff-cc2420-application.h"

namespace ns3 {

class DataRate;

/**
 * \brief A helper to make it easier to instantiate an ns3::OnOffCC2420Application
 * on a set of nodes.
 */
class OnOffCC2420Helper
{
public:
  /**
   * Create an OnOffCC2420Helper to make it easier to work with OnOffCC2420Applications
   */
  OnOffCC2420Helper ();

  /**
   * Helper function used to set the underlying application attributes.
   *
   * \param name the name of the application attribute to set
   * \param value the value of the application attribute to set
   */
  void SetAttribute (std::string name, const AttributeValue &value);

  /**
   * Helper function to set a constant rate source.  Equivalent to
   * setting the attributes OnTime to constant 1000 seconds, OffTime to 
   * constant 0 seconds, and the DataRate and PacketSize set accordingly
   *
   * \param dataRate DataRate object for the sending rate
   * \param packetSize size in bytes of the packet payloads generated
   */
  void SetConstantRate (DataRate dataRate, uint32_t packetSize = 512);

  /**
   * Install an ns3::OnOffCC2420Application on each node of the input container
   * configured with all the attributes set with SetAttribute.
   *
   * \param c NodeContainer of the set of nodes on which an OnOffCC2420Application
   * will be installed.
   * \returns Container of Ptr to the applications installed.
   */
  ApplicationContainer Install (NodeContainer c) const;

  /**
   * Install an ns3::OnOffCC2420Application on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an OnOffCC2420Application will be installed.
   * \returns Container of Ptr to the applications installed.
   */
  ApplicationContainer Install (Ptr<Node> node) const;

  /**
   * Install an ns3::OnOffCC2420Application on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param nodeName The node on which an OnOffCC2420Application will be installed.
   * \returns Container of Ptr to the applications installed.
   */
  ApplicationContainer Install (std::string nodeName) const;

 /**
  * Assign a fixed random variable stream number to the random variables
  * used by this model.  Return the number of streams (possibly zero) that
  * have been assigned.  The Install() method should have previously been
  * called by the user.
  *
  * \param stream first stream index to use
  * \param c NodeContainer of the set of nodes for which the OnOffCC2420Application
  *          should be modified to use a fixed stream
  * \return the number of stream indices assigned by this helper
  */
  int64_t AssignStreams (NodeContainer c, int64_t stream);

private:
  /**
   * \internal
   * Install an ns3::OnOffCC2420Application on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an OnOffCC2420Application will be installed.
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node) const;
  std::string m_protocol;
  Address m_remote;
  ObjectFactory m_factory;
};

} // namespace ns3

#endif /* ON_OFF_CC2420_HELPER_H */

