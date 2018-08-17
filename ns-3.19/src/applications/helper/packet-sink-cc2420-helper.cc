#include "packet-sink-cc2420-helper.h"
#include "ns3/string.h"
#include "ns3/names.h"

namespace ns3 {

PacketSinkCC2420Helper::PacketSinkCC2420Helper ()
{
  m_factory.SetTypeId ("ns3::PacketSinkCC2420");
}

void 
PacketSinkCC2420Helper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
PacketSinkCC2420Helper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
PacketSinkCC2420Helper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
PacketSinkCC2420Helper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
PacketSinkCC2420Helper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}

} // namespace ns3
