/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/socket.h"
/* Setup constant values */

int SPINE_COUNT = 1;
int LEAF_COUNT = 2;
int SERVER_COUNT = 4;
int OVERSUB_RATIO = 1;

int MINRTO = 5; //ms

int LEAF_SPINE_PATH_COUNT = SERVER_COUNT / OVERSUB_RATIO / SPINE_COUNT;
int BW_Gbps=10;     // Bandwidth fixed value
uint64_t LINK_CAPACITY_BASE=40000000000;

const int PORT_START = 10000;
const int PORT_END = 50000;
const int PACKET_SIZE = 1400;
const int MEASURED_RTT_US = 25;

const double startTime = 0.0;
const double endTime = 100.0;


int ComputeBdp()
{
  int bdp = BW_Gbps * MEASURED_RTT_US * 1000 / PACKET_SIZE / 8;
  std::cout << "BDP value is " << bdp << " packets" << std::endl;
  return bdp;
}

const int BDP=ComputeBdp();
const int BUFFER_SIZE=4*BDP;

template<typename T>
T rand_range (T min, T max)
{
  return min + ((double)max - min) * rand () / RAND_MAX;
}

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("pfabric example");


void InstallTcpApplication(NodeContainer servers, int senderIndex, int receiverIndex, int flowid)
{
  int tcpFlowSize = 100;

  Ptr<Node> destServer = servers.Get (receiverIndex);
  Ptr<Ipv4> ipv4 = destServer -> GetObject<Ipv4> ();
  Ipv4InterfaceAddress destInterface = ipv4 -> GetAddress (1,0);
  Ipv4Address destAddress = destInterface.GetLocal();

  uint16_t port = rand_range(PORT_START, PORT_END);
  PfabricBulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (destAddress, port));

  source.SetAttribute ("SendSize", UintegerValue (PACKET_SIZE));
  source.SetAttribute ("MaxBytes", UintegerValue (tcpFlowSize));
  source.SetAttribute ("FlowId", UintegerValue (flowid));


  ApplicationContainer sourceApp = source.Install (servers.Get (senderIndex));
  sourceApp.Start (NanoSeconds (startTime));
  sourceApp.Stop (Seconds (endTime));

  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install (servers.Get (receiverIndex));
  sinkApp.Start (Seconds(startTime));
  sinkApp.Stop (Seconds(endTime));
}



void InstallApplications (NodeContainer servers)
{
  NS_LOG_INFO ("Install Applications");

  int totalServerCount = LEAF_COUNT * SERVER_COUNT;
  std::vector<int> indexVector;
  int flowid = 0;

  for (int senderIndex = 0; senderIndex < totalServerCount; ++senderIndex) 
  {
    for (int receiverIndex = 0; receiverIndex < totalServerCount; ++receiverIndex)
    {
      if (senderIndex == receiverIndex) continue;
      
      flowid++;
      InstallTcpApplication(servers, senderIndex, receiverIndex, flowid);
    } 
  }
}

/* Utility functions */
void ConfigTcp()
{
  NS_LOG_INFO ("Enabling Tcp");

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(PACKET_SIZE));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (1000)));
  Config::SetDefault ("ns3::TcpSocket::PersistTimeout", TimeValue (MilliSeconds (1000)));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (MINRTO)));
  Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));

  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (20)));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));

  // RED Configuration
  Config::SetDefault ("ns3::PfabricQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  Config::SetDefault ("ns3::PfabricQueueDisc::MeanPktSize", UintegerValue (PACKET_SIZE));


  Config::SetDefault ("ns3::PfabricQueueDisc::QueueLimit", UintegerValue (BUFFER_SIZE));
  
  return;
}

void ConfigTopology(NodeContainer& spines,
                             NodeContainer& leaves,
                             NodeContainer& servers,
                             InternetStackHelper& internet,
                             PointToPointHelper& p2p,
                             Ipv4AddressHelper& ipv4,
                             TrafficControlHelper& tc)
{
  spines.Create (SPINE_COUNT);
  leaves.Create (LEAF_COUNT);
  servers.Create (SERVER_COUNT * LEAF_COUNT);

  Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting",
        BooleanValue (true));

  Ipv4GlobalRoutingHelper globalRoutingHelper;
  internet.SetRoutingHelper (globalRoutingHelper);

  internet.Install (servers);
  internet.Install (spines);
  internet.Install (leaves);

  tc.SetRootQueueDisc ("ns3::PfabricQueueDisc");
  Config::SetDefault ("ns3::TcpSocketBase::ReTxThreshold", UintegerValue (0)); // disable dupack (the default setting in pfabric)
  

  NS_LOG_INFO ("Configuring servers");
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LINK_CAPACITY_BASE)));
  p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(1)));
  p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(BUFFER_SIZE));
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
}

void ConfigNetwork(std::vector<Ipv4Address>& leafNetworks,
                   std::vector<Ipv4Address>& serverAddresses,
                   Ipv4AddressHelper& ipv4,
                   NodeContainer& spines,
                   NodeContainer& leaves,
                   NodeContainer& servers,
                   PointToPointHelper& p2p,
                   TrafficControlHelper& tc)
{
  for (int i = 0; i < LEAF_COUNT; ++i) {
    Ipv4Address network = ipv4.NewNetwork ();
    leafNetworks[i] = network;
    
    for (int j = 0; j < SERVER_COUNT; ++j) {
      int serverIndex = i* SERVER_COUNT + j;
      NodeContainer nodeContainer = NodeContainer (leaves.Get (i),
                                                   servers.Get (serverIndex));
      p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LINK_CAPACITY_BASE)));
      p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(10)));
      p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(BUFFER_SIZE));
      NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
      tc.Install (netDeviceContainer);
      Ipv4InterfaceContainer interfaceContainer = ipv4.Assign (netDeviceContainer);
      ipv4.NewNetwork();
      
      NS_LOG_INFO ("Leaf - " << i << " is connected to Server - " << j
                   << " with address " << interfaceContainer.GetAddress(0)
                   << " <-> " << interfaceContainer.GetAddress (1)
                   << " with port " << netDeviceContainer.Get (0)->GetIfIndex ()
                   << " <-> " << netDeviceContainer.Get (1)->GetIfIndex ());

      serverAddresses [serverIndex] = interfaceContainer.GetAddress (1);

      //std::cout << serverIndex << " " << serverAddresses [serverIndex] << std::endl;
    }
    //std::cout << LEAF_SPINE_PATH_COUNT << std::endl;

    for (int k = 0; k < SPINE_COUNT; ++k) {
      for (int l = 0; l < LEAF_SPINE_PATH_COUNT; ++l) {
        NodeContainer nodeContainer = NodeContainer (leaves.Get (i), spines.Get(k));
        p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (LINK_CAPACITY_BASE)));
        p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(1)));
        p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(BUFFER_SIZE));
        NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);        
        tc.Install (netDeviceContainer);
        Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
        ipv4.NewNetwork();
      }
    }
  }
}

void RunSimulation()
{
  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (endTime));
  Simulator::Run ();
}


/**
 * \brief Set monitor names and generate output
 */
void OutputMonitor (Ptr<FlowMonitor> flowMonitor)
{
  std::stringstream flowMonitorFilename;
  flowMonitorFilename << "pfabric.xml";
  flowMonitor->SerializeToXmlFile(flowMonitorFilename.str (), true, true);
}


int main(int argc, char** argv)
{
  ConfigTcp();
  LINK_CAPACITY_BASE=(uint64_t(BW_Gbps)*1000000000);
  LEAF_SPINE_PATH_COUNT = SERVER_COUNT / OVERSUB_RATIO / SPINE_COUNT;
  
  NS_LOG_INFO ("Setting up spine-leaf topology");
  NodeContainer spines, leaves, servers;
  InternetStackHelper internet;
  PointToPointHelper p2p;
  Ipv4AddressHelper ipv4;
  TrafficControlHelper tc;

  ConfigTopology (spines, leaves, servers, internet,
                           p2p, ipv4, tc);
  
  std::vector<Ipv4Address> leafNetworks (LEAF_COUNT);
  std::vector<Ipv4Address> serverAddresses (SERVER_COUNT * LEAF_COUNT);

  ConfigNetwork(leafNetworks, serverAddresses,
                ipv4, spines, leaves, servers, p2p, tc);
 
  NS_LOG_INFO ("Populate global routing tables");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  InstallApplications (servers);
  
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  RunSimulation();
  OutputMonitor(flowMonitor);
  Simulator::Destroy();
  NS_LOG_INFO ("Stop simulation");
  
  return 0;
}