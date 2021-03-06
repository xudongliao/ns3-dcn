#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

#include <sstream>
#include <map>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpTest");

uint32_t checkTimes;
double avgQueueSize;

// attributes
std::string link_data_rate;
std::string link_delay;
uint32_t packet_size;
uint32_t queue_size;
uint32_t threhold;
uint32_t tcpFlowSize = 1000*1024;

// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;
double client_interval_time;

// nodes
NodeContainer nodes;
NodeContainer clients;
NodeContainer switchs;
NodeContainer servers;

// server interfaces
Ipv4InterfaceContainer serverInterfaces;

// receive status
std::map<uint32_t, uint64_t> totalRx;   //fId->receivedBytes
std::map<uint32_t, uint64_t> lastRx;

// throughput result
std::map<uint32_t, std::vector<std::pair<double, double> > > throughputResult; //fId -> list<time, throughput>

QueueDiscContainer queueDiscs;

std::stringstream filePlotQueue;
std::stringstream filePlotQueueAvg;

void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = StaticCast<DctcpQueueDisc> (queue)->GetQueueSize ();

  avgQueueSize += qSize;
  checkTimes++;

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queue);

  std::ofstream fPlotQueue (filePlotQueue.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();

  std::ofstream fPlotQueueAvg (filePlotQueueAvg.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueueAvg << Simulator::Now ().GetSeconds () << " " << avgQueueSize / checkTimes << std::endl;
  fPlotQueueAvg.close ();
}

void
TxTrace (uint32_t flowId, Ptr<const Packet> p)
{
  NS_LOG_FUNCTION (flowId << p);
  FlowIdTag flowIdTag;
  flowIdTag.SetFlowId (flowId);
  p->AddByteTag (flowIdTag);
}

void
RxTrace (Ptr<const Packet> packet, const Address &from)
{
  NS_LOG_FUNCTION (packet << from);
  FlowIdTag flowIdTag;
  bool retval = packet->FindFirstMatchingByteTag (flowIdTag);
  NS_ASSERT (retval);
  if (totalRx.find (flowIdTag.GetFlowId ()) != totalRx.end ())
    {
      totalRx[flowIdTag.GetFlowId ()] += packet->GetSize ();
    }
  else
    {
      totalRx[flowIdTag.GetFlowId ()] = packet->GetSize ();
      lastRx[flowIdTag.GetFlowId ()] = 0;
    }
}

void
CalculateThroughput (void)
{
  for (auto it = totalRx.begin (); it != totalRx.end (); ++it)
    {
      double cur = (it->second - lastRx[it->first]) * (double) 8/1e5; /* Convert Application RX Packets to MBits. */
      throughputResult[it->first].push_back (std::pair<double, double> (Simulator::Now ().GetSeconds (), cur));
      lastRx[it->first] = it->second;
    }
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

void
SetName (void)
{
  // add name to clients
  int i = 0;
  for(auto it = clients.Begin (); it != clients.End (); ++it)
    {
      std::stringstream ss;
      ss << "CL" << i++;
      Names::Add (ss.str (), *it);
    }
  i = 0;
  for(auto it = switchs.Begin (); it != switchs.End (); ++it)
    {
      std::stringstream ss;
      ss << "SW" << i++;
      Names::Add (ss.str (), *it);
    }
  i = 0;
  for(auto it = servers.Begin (); it != servers.End (); ++it)
    {
      std::stringstream ss;
      ss << "SE" << i++;
      Names::Add (ss.str (), *it);
    }
}

void
BuildTopo (uint32_t clientNo, uint32_t serverNo)
{
  NS_LOG_INFO ("Create nodes");
  clients.Create (clientNo);
  switchs.Create (2);
  servers.Create (serverNo);

  SetName ();

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (clients);
  internet.Install (switchs);
  internet.Install (servers);


  /*TrafficControlHelper tchPfifo;
  // use default limit for pfifo (1000)
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (queue_size));
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (queue_size));*/

  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::DctcpQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                           "LinkDelay", StringValue (link_delay));

  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue (link_data_rate));
  p2p.SetChannelAttribute ("Delay", StringValue (link_delay));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  for (auto it = clients.Begin (); it != clients.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (*it, switchs.Get (0)));
      tchRed.Install (devs);
      ipv4.Assign (devs);
      ipv4.NewNetwork ();
    }
  for (auto it = servers.Begin (); it != servers.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (switchs.Get (1), *it));
      tchRed.Install (devs);
      serverInterfaces.Add (ipv4.Assign (devs).Get (1));
      ipv4.NewNetwork ();
    }
  {
    p2p.SetQueue ("ns3::DropTailQueue");
    p2p.SetDeviceAttribute ("DataRate", StringValue (link_data_rate));
    p2p.SetChannelAttribute ("Delay", StringValue (link_delay));
    NetDeviceContainer devs = p2p.Install (switchs);
    // only backbone link has RED queue disc
    queueDiscs = tchRed.Install (devs);
    ipv4.Assign (devs);
  }
  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void
BuildAppsTest (void)
{
  // SINK is in the right side
  NS_LOG_INFO ("init apps");
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (servers.Get (0));
  sinkApp.Start (Seconds (sink_start_time));
  sinkApp.Stop (Seconds (sink_stop_time));
  sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));

  // Connection one
  // Clients are in left side
  /*
   * Create the OnOff applications to send TCP to the server
   * onoffhelper is a client that send data to TCP destination
   */

  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
  clientHelper.SetAttribute ("SendSize", UintegerValue (packet_size));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (tcpFlowSize));

  ApplicationContainer clientApps = clientHelper.Install (clients);

  // set different start/stop time for each app
  double clientStartTime = client_start_time;
  double clientStopTime = client_stop_time;
  uint32_t i = 1;
  for (auto it = clientApps.Begin (); it != clientApps.End (); ++it)
    {
      Ptr<Application> app = *it;
      app->SetStartTime (Seconds (clientStartTime));
      app->SetStopTime (Seconds (clientStopTime));
      app->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, i++));
    }
}

void
SetConfig ()
{

  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));
  // RED params
  NS_LOG_INFO ("Set RED params");
  Config::SetDefault ("ns3::DctcpQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  Config::SetDefault ("ns3::DctcpQueueDisc::MeanPktSize", UintegerValue (packet_size));
  // Config::SetDefault ("ns3::DctcpQueueDisc::Gentle", BooleanValue (false));
  // Config::SetDefault ("ns3::DctcpQueueDisc::QW", DoubleValue (1.0));
  //Config::SetDefault ("ns3::DctcpQueueDisc::UseMarkP", BooleanValue (true));
  //Config::SetDefault ("ns3::DctcpQueueDisc::MarkP", DoubleValue (2.0));
  Config::SetDefault ("ns3::DctcpQueueDisc::MinTh", DoubleValue (threhold));
  Config::SetDefault ("ns3::DctcpQueueDisc::MaxTh", DoubleValue (threhold));
  Config::SetDefault ("ns3::DctcpQueueDisc::QueueLimit", UintegerValue (queue_size));

  // TCP params
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packet_size));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
}

int
main (int argc, char *argv[])
{

  std::string pathOut;
  bool writeForPlot = false;
  bool writePcap = false;
  bool flowMonitor = false;
  bool writeThroughput = false;

  bool printRedStats = true;

  global_start_time = 0.0;
  global_stop_time = 2.0;
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time + 3.0;
  client_start_time = sink_start_time + 0.2;
  client_stop_time = global_stop_time;

  link_data_rate = "1000Mbps";
  link_delay = "50us";
  packet_size = 1024;
  queue_size = 250;
  threhold = 20;

  // Will only save in the directory if enable opts below
  pathOut = "."; // Current directory
  CommandLine cmd;
  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("writeForPlot", "<0/1> to write results for plot (gnuplot)", writeForPlot);
  cmd.AddValue ("writePcap", "<0/1> to write results in pcapfile", writePcap);
  cmd.AddValue ("writeFlowMonitor", "<0/1> to enable Flow Monitor and write their results", flowMonitor);
  cmd.AddValue ("writeThroughput", "<0/1> to write throughtput results", writeThroughput);

  cmd.Parse (argc, argv);

  SetConfig ();
  BuildTopo (3, 1);
  BuildAppsTest ();

  if (writePcap)
    {
      PointToPointHelper ptp;
      std::stringstream stmp;
      stmp << pathOut << "/dctcp";
      ptp.EnablePcapAll (stmp.str ());
    }

  NS_LOG_INFO ("flowm1");

  Ptr<FlowMonitor> flowmon;
  if (flowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

  Simulator::Stop (Seconds (sink_stop_time));
  Simulator::Run ();
  

  NS_LOG_INFO ("flowm2");

  if (writeForPlot)
    {
      filePlotQueue << pathOut << "/" << "dctcp-queue.plotme";
      filePlotQueueAvg << pathOut << "/" << "dctcp-queue_avg.plotme";

      remove (filePlotQueue.str ().c_str ());
      remove (filePlotQueueAvg.str ().c_str ());
      Ptr<QueueDisc> queue = queueDiscs.Get (0);
      Simulator::ScheduleNow (&CheckQueueSize, queue);
    }

  if (writeThroughput)
    {
      Simulator::Schedule (Seconds (sink_start_time), &CalculateThroughput);
    }


  if (flowMonitor)
    {
      std::stringstream stmp;
      stmp << pathOut << "/dctcp.xml";
      NS_LOG_INFO ("flowm3");

      flowmon->SerializeToXmlFile (stmp.str (), false, false);
    }


  /*if (printRedStats)
    {
      DctcpQueueDisc::Stats st = StaticCast<DctcpQueueDisc> (queueDiscs.Get (0))->GetStats ();
      std::cout << "*** RED stats from Node 2 queue ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due prob mark" << std::endl;
      std::cout << "\t " << st.unforcedMark << " marks due prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due hard mark" << std::endl;
      std::cout << "\t " << st.qLimDrop << " drops due queue full" << std::endl;

      st = StaticCast<DctcpQueueDisc> (queueDiscs.Get (1))->GetStats ();
      std::cout << "*** RED stats from Node 3 queue ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due prob mark" << std::endl;
      std::cout << "\t " << st.unforcedMark << " marks due prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due hard mark" << std::endl;
      std::cout << "\t " << st.qLimDrop << " drops due queue full" << std::endl;
    }*/

  if (writeThroughput)
    {
      for (auto& resultList : throughputResult)
        {
          std::stringstream ss;
          ss << pathOut << "/throughput-" << resultList.first << ".txt";
          std::ofstream out (ss.str ());
          for (auto& entry : resultList.second)
            {
              out << entry.first << "," << entry.second << std::endl;
            }
        }
    }
  Simulator::Destroy ();

  return 0;
}