#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/processing-module.h"

// From nictest.cc:
#if 0
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/processing-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <fstream>
#include <sys/time.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#endif

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("TestProcMod");

static double duration = 10;
// static int size = 100;
static int pps = 1;

namespace ns3 {
    // For debug
    extern bool debugOn;
    extern bool traceOn;
    extern bool withBlockingIO;
}

class DummyProtocolStack {
public:
	void DriverTransmit(Ptr<Packet> p, Ptr<Node> n);
	void DriverReceive(Ptr<Packet> p, Ptr<Node> n);
	void IPReceive(Ptr<Packet> p, Ptr<Node> n);
	void GenerateTraffic(Ptr<Node> n, uint32_t pktSize);
	void GeneratePacket(Ptr<Node> n, uint32_t pktSize, uint32_t curSeqNr);
};

void DummyProtocolStack::DriverReceive(Ptr<Packet> p, Ptr<Node> n) {
	Ptr<ExecEnv> ee = n->GetObject<ExecEnv>();
	p->m_executionInfo.executedByExecEnv = false;

	std::cout << Simulator::Now() << ": " << "Driver receives packet " << p->m_executionInfo.seqNr << std::endl;

	p->m_executionInfo.timestamps.push_back(Simulator::Now());
	ee->Proceed(p, "ip::receive", &DummyProtocolStack::IPReceive, this, p, n);
}

void DummyProtocolStack::IPReceive(Ptr<Packet> p, Ptr<Node> n) {
	Ptr<ExecEnv> ee = n->GetObject<ExecEnv>();
	p->m_executionInfo.executedByExecEnv = false;

	std::cout << Simulator::Now() << ": " << "IP receives packet " << p->m_executionInfo.seqNr << std::endl;

	p->m_executionInfo.timestamps.push_back(Simulator::Now());
	ee->Proceed(p, "nicdriver::transmit", &DummyProtocolStack::DriverTransmit, this, p, n);

}

void DummyProtocolStack::DriverTransmit(Ptr<Packet> p, Ptr<Node> n) {
	Ptr<ExecEnv> ee = n->GetObject<ExecEnv>();
	p->m_executionInfo.executedByExecEnv = false;

	std::cout << Simulator::Now() << ": " << "Driver transmits packet " << p->m_executionInfo.seqNr << std::endl;

	// ns3::packetsReceived++;
}

void schedule_interrupt(Ptr<Node> node) {
    Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();

    ProgramLocation *dummyProgramLoc = new ProgramLocation;
    dummyProgramLoc = new ProgramLocation;
    dummyProgramLoc->tempvar = tempVar();
    dummyProgramLoc->curPkt = Ptr<Packet>();
    dummyProgramLoc->localStateVariables = std::map<std::string, Ptr<StateVariable> >();
    dummyProgramLoc->localStateVariableQueues = std::map<std::string, Ptr<StateVariableQueue> >();

    static int cpu = 0;

    Simulator::ScheduleNow(&InterruptController::IssueInterruptWithServiceOnCPU,
            ee->hwModel->m_interruptController,
            cpu, // cpu
            ee->m_serviceMap["HIRQ-24"],
            dummyProgramLoc);

    cpu = (cpu + 1) % 2;

    // Simulator::Schedule(Seconds(1), &schedule_interrupt, node);
}

void DummyProtocolStack::GeneratePacket(Ptr<Node> n, uint32_t pktSize, uint32_t curSeqNr) {
	Ptr<ExecEnv> ee = n->GetObject<ExecEnv>();
	Ptr<Packet> toSend = Create<Packet>(pktSize);

	toSend->m_executionInfo.seqNr = curSeqNr;
	toSend->m_executionInfo.executedByExecEnv = false;
	ee->Proceed(toSend, "nicdriver::receive", &DummyProtocolStack::DriverReceive, this, toSend, n);
	toSend->m_executionInfo.timestamps.push_back(Simulator::Now());
	ee->queues["nic_rx"]->Enqueue(toSend);

    /*
	Simulator::ScheduleNow(&InterruptController::IssueInterruptWithService,
			ee->hwModel->m_interruptController, ee->m_serviceMap["HIRQ-123"],
			tempVar(), Ptr<Packet>(),
			std::map<std::string, Ptr<StateVariable> >(),
			std::map<std::string, Ptr<StateVariableQueue> >());
            */

    schedule_interrupt(n);

	// ns3::packetsSent++;
}

void DummyProtocolStack::GenerateTraffic(Ptr<Node> n, uint32_t pktSize) {
	static int curSeqNr = 0;
	GeneratePacket(n, pktSize, curSeqNr++);
	if (Simulator::Now().GetSeconds() + (1.0 / (double) pps) < duration)
		Simulator::Schedule(Seconds(1.0 / (double) pps),
				&DummyProtocolStack::GenerateTraffic, this, n, pktSize);
}

int main(int argc, char *argv[]) {
    // Set up parameters
    uint32_t seed = 1;

    // Debugging and tracing
    ns3::debugOn = false;
    ns3::traceOn = false;
    ns3::withBlockingIO = true;

    // Fetch from command line
    CommandLine cmd;
    cmd.AddValue("seed", "seed for the random generator", seed);
    cmd.AddValue("duration", "The number of seconds the simulation should run", duration);
    cmd.AddValue("trace", "Trace parsing of device file, and execution of SEM", ns3::traceOn);
    cmd.Parse(argc, argv);

    SeedManager::SetSeed(seed);

    // Create node with ExecEnv
    NodeContainer c;
    c.Create(1);
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(289));

    // eeh->Install("tiny.device", c.Get(0));
    // eeh->Install("semaphoretest.device", c.Get(0));
    eeh->Install("compltest.device", c.Get(0));

    Simulator::Schedule(Seconds(1), &schedule_interrupt, c.Get(0));
    
#if 0
	DummyProtocolStack *tester = new DummyProtocolStack();
	Simulator::Schedule(Seconds(0), &DummyProtocolStack::GenerateTraffic, tester, c.Get(0), size);
#endif

    // Set up and start simulation
    Simulator::Stop(Seconds((double) duration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
