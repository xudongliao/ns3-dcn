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
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PiasExample");

uint32_t numSpine = 4;	
uint32_t numLeaf = 9;
uint32_t numServerPerLeaf = 16;
uint64_t leafSpineSpeed = 40000000000;
uint64_t leafServerSpeed = 10000000000;
int      overSubRatio = 1;
int minRto = 5;
int baseRttUs = 25;
int packetSize = 1400; //!< in bytes  
int bufferSize = 4 * leafServerSpeed * baseRttUs * 1e-6 / packetSize / 8; //!< in packets 
const int PORT_START = 10000;
const int PORT_END = 50000;

// Leaf-spine Path Count 
int pathLeafSpine = numServerPerLeaf / overSubRatio / numSpine;

// simulation time
const double startTime = 0.0;
const double endTime = 100.0;

void 
ConfigTopology(NodeContainer& spines, NodeContainer& leaves,
               NodeContainer& servers, InternetStackHelper& internet,
               PointToPointHelper& p2p, Ipv4AddressHelper& ipv4, 
               TrafficControlHelper& tc)
{
  spines.Create (numSpine);
  leaves.Create (numLeaf);
  servers.Create (numServerPerLeaf);
  NS_LOG_UNCOND ("Spine: " << spines.GetN() << "; Leaves: " 
                 << leaves.GetN() << "; serversPerLeaf: " 
                 << servers.GetN());
  // enable routing 
  Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
  internet.Install (servers);
  internet.Install (spines);
  internet.Install (leaves);

  NS_LOG_INFO ("Configuring Priority Queue Desc with 8 internal FIFO queues");
  uint16_t handle = tc.SetRootQueueDisc ("ns3::PrioQueueDisc", 
                                         "Priomap", StringValue("0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7"),
                                         "EcnMarkingScheme",UintegerValue(ns3::PRIO_PER_PORT_ECN), // per port
                                         "EcnThreshold", UintegerValue(65) // DCTCP_K
                                         );
  TrafficControlHelper::ClassIdList cid = tc.AddQueueDiscClasses (handle, 8, "ns3::QueueDiscClass");
  for (int i = 0; i < 8; i++)
    {
      tc.AddChildQueueDisc (handle, cid[i], "ns3::FifoQueueDisc");
    }
  NS_LOG_INFO ("Configuring servers");
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (leafServerSpeed)));
  p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(1)));
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
}

void ConfigNetwork(std::vector<Ipv4Address>& leafNetworks,
                   std::vector<Ipv4Address>& serverAddresses,
                   NodeContainer& spines, NodeContainer& leaves,
                   NodeContainer& servers, PointToPointHelper& p2p,
                   Ipv4AddressHelper& ipv4, TrafficControlHelper& tc)
{
  for (int i = 0; i < numLeaf; i++)
    {
      Ipv4Address network = ipv4.NewNetwork ();
      leafNetworks[i] = network;
      for (int j = 0; j < numServerPerLeaf; j++)
        {
          int serverIndex = i * numServerPerLeaf + j;
          NodeContainer nodeContainer = NodeContainer (leaves.Get (i), servers.Get (serverIndex));
          p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (leafServerSpeed)));
          p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(20)));
          p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize(ns3::PACKETS, bufferSize)));
          NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);
          tc.Install (netDeviceContainer);
          Ipv4InterfaceContainer interfaceContainer = ipv4.Assign (netDeviceContainer);
          ipv4.NewNetwork();
          NS_LOG_UNCOND ("Leaf - " << i << " is connected to Server - " << j
                   << " with address " << interfaceContainer.GetAddress(0)
                   << " <-> " << interfaceContainer.GetAddress (1)
                   << " with port " << netDeviceContainer.Get (0)->GetIfIndex ()
                   << " <-> " << netDeviceContainer.Get (1)->GetIfIndex ());
          serverAddresses [serverIndex] = interfaceContainer.GetAddress (1);
        }
        for (int k = 0; k < numSpine; k++)
          {
            for (int l = 0; l < pathLeafSpine; l++)
              {
                NodeContainer nodeContainer = NodeContainer (leaves.Get (i), spines.Get(k));
                p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (leafSpineSpeed)));
                p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(10)));
                p2p.SetQueue ("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize(ns3::PACKETS, bufferSize)));
                NetDeviceContainer netDeviceContainer = p2p.Install (nodeContainer);        
                tc.Install (netDeviceContainer);
                Ipv4InterfaceContainer ipv4InterfaceContainer = ipv4.Assign (netDeviceContainer);
                ipv4.NewNetwork();
              }
          }
    }
}

template<typename T>
T rand_range (T min, T max)
{
  return min + ((double)max - min) * rand () / RAND_MAX;
}

void InstallTcpApplication(NodeContainer servers, int senderIndex, int receiverIndex, int flowid)
{
  // int tcpFlowSize = 100;

  Ptr<Node> destServer = servers.Get (receiverIndex);
  Ptr<Ipv4> ipv4 = destServer -> GetObject<Ipv4> ();
  Ipv4InterfaceAddress destInterface = ipv4 -> GetAddress (1,0);
  Ipv4Address destAddress = destInterface.GetLocal();

  uint16_t port = rand_range(PORT_START, PORT_END);
  PiasBulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (destAddress, port));

  source.SetAttribute ("SendSize", UintegerValue (packetSize));
  source.SetAttribute ("MaxBytes", StringValue ("100KB"));
  // source.SetAttribute ("FlowId", UintegerValue (flowid));


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

  int totalServerCount = numLeaf * numServerPerLeaf;
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

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (0));
  Config::SetDefault ("ns3::TcpSocket::ConnTimeout", TimeValue (MilliSeconds (1000)));
  Config::SetDefault ("ns3::TcpSocket::PersistTimeout", TimeValue (MilliSeconds (1000)));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (10));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (MilliSeconds (minRto)));
  Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (100)));

  Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue (MicroSeconds (20)));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (160000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (160000000));
  Config::SetDefault ("ns3::TcpSocketBase::ReTxThreshold", UintegerValue (0)); // disable dupack (the default setting in pfabric)
  
  return;
}

void RunSimulation()
{
  NS_LOG_INFO ("Start simulation");
  Simulator::Stop (Seconds (endTime));
  Simulator::Run ();
}

void OutputMonitor (Ptr<FlowMonitor> flowMonitor)
{
  std::stringstream flowMonitorFilename;
  flowMonitorFilename << "pias.xml";
  flowMonitor->SerializeToXmlFile(flowMonitorFilename.str (), true, true);
}

int
main (int argc, char *argv[])
{
  NS_LOG_UNCOND ("Start PIAS Example");
  CommandLine cmd;
  cmd.AddValue ("numSpine", "Number of spine switches", numSpine);
  cmd.AddValue ("numLeaf", "Number of leaf switches", numLeaf);
  cmd.AddValue ("numServerPerLeaf", "Number of servers per leaf switch", numServerPerLeaf);
  cmd.AddValue ("leafSpineSpeed", "Link Speed between leaf and spind switches", leafSpineSpeed);
  cmd.AddValue ("leafServerSpeed", "Link Speed between leaf switch and server", leafServerSpeed);
  cmd.AddValue("overSubRatio", "Oversubscription Ratio in the datacenter networks", overSubRatio);
  cmd.Parse (argc, argv);
  
  NS_LOG_UNCOND ("Create P2P");
  // Create point-to-point channel helper
  PointToPointHelper p2pLeafSpine;
  p2pLeafSpine.SetDeviceAttribute ("DataRate", DataRateValue(DataRate(leafSpineSpeed)));
  p2pLeafSpine.SetChannelAttribute("Delay", TimeValue(MicroSeconds(40)));
  
  NS_LOG_UNCOND ("Setting up spine-leaf topology");
  NodeContainer spines, leaves, servers;
  InternetStackHelper internet;
  PointToPointHelper p2p;
  Ipv4AddressHelper ipv4;
  TrafficControlHelper tc;

  NS_LOG_UNCOND ("Create Topo");
  ConfigTopology(spines, leaves, servers, internet, p2p, ipv4, tc);
  std::vector<Ipv4Address> leafNetworks (numLeaf);
  std::vector<Ipv4Address> serverAddresses (numServerPerLeaf * numLeaf);

  NS_LOG_UNCOND ("Create Network");
  ConfigNetwork (leafNetworks, serverAddresses, spines, leaves, servers, p2p, ipv4, tc);

  NS_LOG_UNCOND ("Populate global routing tables");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  RunSimulation();
  OutputMonitor(flowMonitor);
  Simulator::Destroy();
  NS_LOG_INFO ("Stop simulation");

  return 0;
}

