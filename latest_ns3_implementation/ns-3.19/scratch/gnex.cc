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
    extern bool withBlockingIO;
}

static uint32_t seed = 1;
static double duration = 5;
// static int size = 100;
static int pps = 1;
static bool print = 0;
static std::string deviceFile = "gnex-min.device";

class ProtocolStack {
public:
	void DriverTransmit(Ptr<Packet> p, Ptr<Node> n);
	void DriverReceive(Ptr<Packet> p, Ptr<Node> n);
	void IPReceive(Ptr<Packet> p, Ptr<Node> n);
    void NICReceive(Ptr<Packet> p, Ptr<Node> n);
    void NICSend(Ptr<Packet> p, Ptr<Node> n);

    void MeasureStart(Ptr<Packet> p, Ptr<Node> n);

	void GenerateTraffic(Ptr<Node> n, uint32_t pktSize);
	void GeneratePacket(Ptr<Node> n, uint32_t pktSize, uint32_t curSeqNr);
};

static ProgramLocation *dummyProgramLoc = NULL;
// ScheduleInterrupt schedules an interrupt on the node.
// interruptId is the service name of the interrupt, such as HIRQ-123
void ScheduleInterrupt(Ptr<Node> node, const char* interruptId) {
    Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();

    // TODO: Model the interrupt distribution somehow
    static int cpu = 0;

    dummyProgramLoc = new ProgramLocation();
    dummyProgramLoc->tempvar = tempVar();
    dummyProgramLoc->curPkt = Ptr<Packet>();
    dummyProgramLoc->localStateVariables = std::map<std::string, Ptr<StateVariable> >();
    dummyProgramLoc->localStateVariableQueues = std::map<std::string, Ptr<StateVariableQueue> >();

    Simulator::ScheduleNow(&InterruptController::IssueInterruptWithServiceOnCPU,
            ee->hwModel->m_interruptController,
            cpu, // cpu
            ee->m_serviceMap[interruptId], // HIRQ-123
            dummyProgramLoc);

    // Round robin distribution
    // cpu = (cpu + 1) % 2;
}

void ProtocolStack::IPReceive(Ptr<Packet> packet, Ptr<Node> node) {
	Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
	packet->m_executionInfo.executedByExecEnv = false;

	// std::cout << Simulator::Now() << ": " << "IP receives packet " << p->m_executionInfo.seqNr << std::endl;
    //std::cout << "IPReceive" << std::endl;
    if (print)
        std::cout << Simulator::Now() << ": " << "IP receives packet " << packet->m_executionInfo.seqNr << std::endl;

	// packet->m_executionInfo.timestamps.push_back(Simulator::Now());
	execenv->Proceed(packet, "nicdriver::transmit", &ProtocolStack::DriverTransmit, this, packet, node);
}

void ProtocolStack::DriverReceive(Ptr<Packet> packet, Ptr<Node> node) {
	Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
	packet->m_executionInfo.executedByExecEnv = false;

    if (print)
        std::cout << Simulator::Now() << ": " << "Driver receives packet " << packet->m_executionInfo.seqNr << std::endl;

	// packet->m_executionInfo.timestamps.push_back(Simulator::Now());
	execenv->Proceed(packet, "ip::receive", &ProtocolStack::IPReceive, this, packet, node);
}

void ProtocolStack::MeasureStart(Ptr<Packet> packet, Ptr<Node> node) {
    std::cout << "ProtocolStack::MeasureStart" << std::endl;
	/*Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
	// packet->m_executionInfo.executedByExecEnv = false;
    std::cout << "HERE" << std::endl;

    // NS_ASSERT_MSG(0, "TEST");

	// packet->m_executionInfo.timestamps.push_back(Simulator::Now());
	// execenv->Proceed(packet, "ip::receive", &ProtocolStack::IPReceive, this, packet, node);
    packet->m_executionInfo.executedByExecEnv = false;
	execenv->Proceed(packet, "nicdriver::receive", &ProtocolStack::DriverReceive, this, packet, node);*/
}

void ProtocolStack::DriverTransmit(Ptr<Packet> packet, Ptr<Node> node) {
	Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
	packet->m_executionInfo.executedByExecEnv = false;

    if (print)
        std::cout << Simulator::Now() << ": " << "Driver transmits packet " << packet->m_executionInfo.seqNr << std::endl;

    // std::cout << "DELTA: " << packet->m_executionInfo.timestamps[1] - packet->m_executionInfo.timestamps[0] << std::endl;
	// packet->m_executionInfo.timestamps.push_back(Simulator::Now());

	execenv->Proceed(packet, "nic::transmit", &ProtocolStack::NICSend, this, packet, node);
}

Gnuplot *ppsPlot = NULL;
Gnuplot *delayPlot = NULL;
Gnuplot2dDataset *ppsDataSet = NULL;
Gnuplot2dDataset *delayDataSet = NULL;

void createPlot(Gnuplot** plot, std::string filename, std::string title, Gnuplot2dDataset** dataSet) {
    *plot = new Gnuplot(filename);
    (*plot)->SetTitle(title);
    (*plot)->SetTerminal("png");

    // plot->AppendExtra("set yrange [0:+5000]");

    *dataSet = new Gnuplot2dDataset();
    (*dataSet)->SetTitle(title);
    (*dataSet)->SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void writePlot(Gnuplot* plot, std::string filename, Gnuplot2dDataset* dataSet) {
    plot->AddDataset(*dataSet);
    std::ofstream plotFile(filename.c_str());
    plot->GenerateOutput(plotFile);
    plotFile.close();

    delete plot;
    delete dataSet;
}

static double lastTime = 0;
static int64_t packetsForwarded = 0;

void ProtocolStack::NICSend(Ptr<Packet> packet, Ptr<Node> node) {
    if (print)
        std::cout << Simulator::Now() << ": " << "NIC transmits packet " << packet->m_executionInfo.seqNr << std::endl;

	// packet->m_executionInfo.timestamps.push_back(Simulator::Now());

    packetsForwarded += 1;

    if (Simulator::Now().GetSeconds() - lastTime >= 0.1) {
        ppsDataSet->Add(lastTime, packetsForwarded);
        // lastTime = (int)Simulator::Now().GetSeconds();
        lastTime += 0.1;
        packetsForwarded = 0;
    }

    static int i = 0;
	// delayDataSet->Add(i++, (packet->m_executionInfo.timestamps[4] - packet->m_executionInfo.timestamps[2]).GetNanoSeconds() );
	// delayDataSet->Add(i++, (packet->m_executionInfo.timestamps[4] - packet->m_executionInfo.timestamps[3]).GetNanoSeconds() );
	delayDataSet->Add(i++, (packet->m_executionInfo.timestamps[1] - packet->m_executionInfo.timestamps[0]).GetNanoSeconds() );

    if (print) {
        std::cout << "DELAYS: ";
        for (std::vector<Time>::iterator it = packet->m_executionInfo.timestamps.begin(); it != packet->m_executionInfo.timestamps.end(); ++it ) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;
    }

	// ns3::packetsReceived++;
}

// NICReceive adds the packet to the NIC rx queue, and schedules DriverReceive
// to be executed once the nicdriver::receive trigger is reached.
void ProtocolStack::NICReceive(Ptr<Packet> packet, Ptr<Node> node) {
	Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();

    packet->m_executionInfo.executedByExecEnv = false;
    // When scheduling HIRQ-162 AND enqueueing packet on nic::rx, the below statement has to occur.
    // We are trying to understand exactly why. We believe it will give us the answers on how to use triggers.
    // The program does NOT fail if either of them is commented out. The if-statement below doesn't cause any different behavior.
    // In the case of the trigger below, we believe it to have to do with dequeueing from nic::rx. This would explain
    // why it only happens when we enqueue on nic::rx, because only then will we attempt to dequeue from it.
    //
    // It's not the DEQUEUE event that causes the error. It happens after calling HIRQ-91
	execenv->Proceed(packet, "nicdriver::receive", &ProtocolStack::DriverReceive, this, packet, node);
	// packet->m_executionInfo.timestamps.push_back(Simulator::Now());

    // If the queue was empty before, that means the driver was
    // waiting on a semaphore to be woken up by an interrupt.

    if (execenv->queues["nic::rx"]->IsEmpty()) {
    // if (execenv->queues["nic::Q"]->IsEmpty()) {
        ScheduleInterrupt(node, "HIRQ-162");

        // Simulate interrupt instead?
        // execenv->hwModel->cpus[0]->taskScheduler->SynchRequest(0, 0, "dhd_dpc_sem", std::vector<uint32_t> ());
    }

    /*
    Simulator::ScheduleNow(
            &ScheduleInterrupt,
            node,
            "HIRQ-162");
            */


    // TODO: GetQueue(str) function
	// execenv->queues["nic::Q"]->Enqueue(packet);
    execenv->queues["nic::tx"]->Enqueue(packet);
}

// GeneratePacket creates a packet and passes it on to the NIC
void ProtocolStack::GeneratePacket(Ptr<Node> n, uint32_t pktSize, uint32_t curSeqNr) {

    Ptr<Packet> toSend = Create<Packet>(pktSize);
	toSend->m_executionInfo.seqNr = curSeqNr;
	toSend->m_executionInfo.executedByExecEnv = false;

	Ptr<ExecEnv> execenv = n->GetObject<ExecEnv>();

    if (print)
        std::cout << "Generating packet " << curSeqNr << std::endl;

    NICReceive(toSend, n);
}

// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate. 
void ProtocolStack::GenerateTraffic(Ptr<Node> n, uint32_t pktSize) {
	static int curSeqNr = 0;

	GeneratePacket(n, pktSize, curSeqNr++);

	if (Simulator::Now().GetSeconds() + (1.0 / (double) pps) < duration)
		Simulator::Schedule(Seconds(1.0 / (double) pps),
				&ProtocolStack::GenerateTraffic, this, n, pktSize);
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
    cmd.AddValue("print", "Print events in the protocol stack", print);
    cmd.AddValue("device", "Device file to use for simulation", deviceFile);
    cmd.Parse(argc, argv);

    SeedManager::SetSeed(seed);

    createPlot(&ppsPlot, "testplot.png", "pps", &ppsDataSet);
    createPlot(&delayPlot, "delayplot.png", "intra-os delay", &delayDataSet);

    // Create node with ExecEnv
    NodeContainer c;
    c.Create(1);
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(289));

    eeh->Install(deviceFile, c.Get(0));

    Ptr<ExecEnv> ee = c.Get(0)->GetObject<ExecEnv>();
    ProtocolStack *protocolStack = new ProtocolStack();

    protocolStack->GenerateTraffic(c.Get(0), 100);

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    writePlot(ppsPlot, "testplot.gnu", ppsDataSet);
    writePlot(delayPlot, "delay.gnu", delayDataSet);

    Simulator::Destroy();
    return 0;
}
