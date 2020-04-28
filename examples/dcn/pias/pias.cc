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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SpineLeafAnimation");

uint32_t numSpine = 4;	
uint32_t numLeaf = 9;
uint32_t numServerPerLeaf = 16;
uint64_t leafSpineSpeed = 40000000000;
uint64_t leafServerSpeed = 10000000000;
int      overSubRatio = 1;
int baseRttUs = 25;
int packetSize = 1400; //!< in bytes  
int bufferSize = 4 * leafServerSpeed * baseRttUs * 1e-6 / packetSize / 8; //!< in packets 

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

  // enable routing 
  Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(true));
  internet.Install (servers);
  internet.Install (spines);
  internet.Install (leaves);

  NS_LOG_INFO ("Configuring Priority Queue Desc with 8 internal FIFO queues");
  uint16_t handle = tc.SetRootQueueDisc ("ns3::PrioQueueDisc", 
                                         "Priomap", StringValue("0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7"),
                                         "EcnMarkingScheme",UintegerValue(2), // per port
                                         "EcnThreshold", UintegerValue(65) // DCTCP_K
                                         );
  TrafficControlHelper::ClassIdList cid = tc.AddQueueDiscClasses (handle, 8, "ns3::QueueDiscClass");
  for (int i = 0; i < 8; i++)
    {
      tc.AddChildQueueDisc (handle, cid[i], "ns3::FifoQueueDisc");
    }
  Config::SetDefault ("ns3::TcpSocketBase::ReTxThreshold", UintegerValue (0)); // disable dupack (the default setting in pfabric)
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
          p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(bufferSize));
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
        }
        for (int k = 0; k < numSpine; k++)
          {
            for (int l = 0; l < pathLeafSpine; l++)
              {
                NodeContainer nodeContainer = NodeContainer (leaves.Get (i), spines.Get(k));
                p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (leafSpineSpeed)));
                p2p.SetChannelAttribute ("Delay", TimeValue(MicroSeconds(10)));
                p2p.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(bufferSize));
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

void OutputMonitor (Ptr<FlowMonitor> flowMonitor)
{
  std::stringstream flowMonitorFilename;
  flowMonitorFilename << "pfabric.xml";
  flowMonitor->SerializeToXmlFile(flowMonitorFilename.str (), true, true);
}

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("numSpine", "Number of spine switches", numSpine);
  cmd.AddValue ("numLeaf", "Number of leaf switches", numLeaf);
  cmd.AddValue ("numServerPerLeaf", "Number of servers per leaf switch", numServerPerLeaf);
  cmd.AddValue ("leafSpineSpeed", "Link Speed between leaf and spind switches");
  cmd.AddValue ("leafServerSpeed", "Link Speed between leaf switch and server");
  cmd.AddValue("overSubRatio", "Oversubscription Ratio in the datacenter networks");
  cmd.Parse (argc, argv);

  // Create point-to-point channel helper
  PointToPointHelper p2pLeafSpine;
  p2pLeafSpine.SetDeviceAttribute ("DataRate", DataRateValue(DataRate(leafSpineSpeed)));
  p2pLeafSpine.SetChannelAttribute("Delay", TimeValue(MicroSeconds(40)));
  
  NS_LOG_INFO ("Setting up spine-leaf topology");
  NodeContainer spines, leaves, servers;
  InternetStackHelper internet;
  PointToPointHelper p2p;
  Ipv4AddressHelper ipv4;
  TrafficControlHelper tc;

  ConfigTopology(spines, leaves, servers, internet, p2p, ipv4, tc);
  std::vector<Ipv4Address> leafNetworks (numLeaf);
  std::vector<Ipv4Address> serverAddresses (numServerPerLeaf * numLeaf);
  ConfigNetwork (leafNetworks, serverAddresses, spines, leaves, servers, p2p, ipv4, tc);

  NS_LOG_INFO ("Populate global routing tables");
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

