#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/processing-module.h"

#include <fstream>
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GalaxyNexus");

namespace ns3 {
    // For debug
    extern bool debugOn;
    extern bool traceOn;
    extern unsigned long eventCount;
}

static uint32_t seed = 1;
static double duration = 10;
static std::string deviceFile = "";
static uint32_t nodeCount = 1;
static uint32_t rate = 10000;

void GenerateTraffic(Ptr<Node> n) {
    // std::cout << "Generating packet" << std::endl;
    Ptr<ExecEnv> ee = n->GetObject<ExecEnv>();
    ee->queues["input"]->Enqueue(Create<Packet>());

	if (Simulator::Now().GetSeconds() + (1.0 / (double) rate) < duration)
		Simulator::Schedule(Seconds(1.0 / (double) rate), &GenerateTraffic, n);
}

int main(int argc, char *argv[])
{
    // Fetch from command line
    CommandLine cmd;
    cmd.AddValue("seed", "seed for the random generator", seed);
    cmd.AddValue("duration", "The number of seconds the simulation should run", duration);
    cmd.AddValue("trace", "Trace parsing of device file, and execution of SEM", ns3::traceOn);
    cmd.AddValue("device", "Name of device file to use", deviceFile);
    cmd.AddValue("count", "Number of nodes", nodeCount);
    cmd.AddValue("rate", "Rate in packets per second", rate);
    cmd.Parse(argc, argv);

    if (deviceFile == "") {
        std::cout << "Missing required argument --device <file>" << std::endl;
        return 0;
    }

    SeedManager::SetSeed(seed);

    // Create node with ExecEnv
    NodeContainer c;
    c.Create(nodeCount);

    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));

    eeh->Install(deviceFile, c);

    for (NodeContainer::Iterator it = c.Begin(); it != c.End(); it++) {
        Ptr<Node> n = *it;
        GenerateTraffic(n);
    }

    eventCount = 0;

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    Simulator::Destroy();

    std::cout << "Event count: " << eventCount << std::endl;

    return 0;
}
