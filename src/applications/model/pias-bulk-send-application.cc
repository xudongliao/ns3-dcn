#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "pias-bulk-send-application.h"
#include <algorithm>

namespace ns3{

NS_LOG_COMPONENT_DEFINE ("PiasBulkSendApplication");

NS_OBJECT_ENSURE_REGISTERED(PiasBulkSendApplication);

ATTRIBUTE_HELPER_CPP (PiasThreshold);

std::ostream &
operator << (std::ostream &os, const PiasThreshold &threshs)
{
  std::copy (threshs.begin (), threshs.end ()-1, std::ostream_iterator<uint32_t>(os, " "));
  os << threshs.back ();
  return os;
}

std::istream &operator >> (std::istream &is, PiasThreshold &threshs)
{
  for (int i = 0; i < 7; i++)
    {
      if (!(is >> threshs[i]))
        {
          NS_FATAL_ERROR ("Incomplete PiasThresholds specification (" << i << " values provided, 16 required)");
        }
    }
  return is;
}

TypeId
PiasBulkSendApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PiasBulkSendApplication")
    .SetParent<Application> ()
    .SetGroupName ("Application")
    .AddConstructor<PiasBulkSendApplication> ()
    .AddAttribute ("PiasPriorityNumber", 
                   "The number of PIAS priorities", 
                   UintegerValue(8),
                   MakeUintegerAccessor (&PiasBulkSendApplication::m_piasPrioNum),
                   MakeUintegerChecker<uint16_t> (1))
    .AddAttribute ("PiasThresholds", "The PIAS threshold to assign priority.",
                   PiasThresholdValue (PiasThreshold{{750*MEAN_PACKET_SIZE, 
                   1132*MEAN_PACKET_SIZE, 1456*MEAN_PACKET_SIZE,
                   1737*MEAN_PACKET_SIZE, 2010*MEAN_PACKET_SIZE, 
                   2199*MEAN_PACKET_SIZE, 2325*MEAN_PACKET_SIZE}}),
                   MakePiasThresholdAccessor (&PiasBulkSendApplication::m_threshs),
                   MakePiasThresholdChecker ())
    .AddAttribute ("SendSize", 
                   "The amount of data sent each time", 
                   UintegerValue(500),
                   MakeUintegerAccessor (&PiasBulkSendApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&PiasBulkSendApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PiasBulkSendApplication::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DelayThresh",
                   "How many packets can pass before we have delay, 0 for disable",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PiasBulkSendApplication::m_delayThresh),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DelayTime",
                   "The time for a delay",
                   TimeValue (MicroSeconds (100)),
                   MakeTimeAccessor (&PiasBulkSendApplication::m_delayTime),
                   MakeTimeChecker())
    .AddAttribute ("SimpleTOS",
                   "A simple version of TOS",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PiasBulkSendApplication::m_tos),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&PiasBulkSendApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&PiasBulkSendApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddAttribute ("FlowId",
                   "id of the flow",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PiasBulkSendApplication::m_flowid),
                   MakeUintegerChecker<uint32_t> ())

  ;
  return tid;
}

PiasBulkSendApplication::PiasBulkSendApplication ()
{
  NS_LOG_FUNCTION (this);
  m_sendSize = 0;
  m_socket = 0;
  m_totBytes = 0;
  m_isDelay = false;
  m_accumPackets = 0;
}

PiasBulkSendApplication::~PiasBulkSendApplication ()
{
  NS_LOG_FUNCTION (this);
}

void 
PiasBulkSendApplication::SetMaxBytes (uint32_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket> 
PiasBulkSendApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
PiasBulkSendApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

// Application Methods
void 
PiasBulkSendApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), m_tid);

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                          "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }

      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();
      m_socket->SetConnectCallback (
        MakeCallback (&PiasBulkSendApplication::ConnectionSucceeded, this),
        MakeCallback (&PiasBulkSendApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&PiasBulkSendApplication::DataSend, this));
    }
  if (m_connected)
    {
      SendData ();
    }
}

void
PiasBulkSendApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("BulkSendApplication found null socket to close in StopApplication");
    }
}

void
PiasBulkSendApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);

  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more

      // uint64_t to allow the comparison later.
      // the result is in a uint32_t range anyway, because
      // m_sendSize is uint32_t.
      uint64_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (toSend, m_maxBytes - m_totBytes);
        }

      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      // Assign priority 
      SocketPriorityTag priorityTag;
      priorityTag.SetPriority (PiasPriority (m_totBytes));
      int actual = m_socket->Send (packet);
      if (actual > 0)
        {
          m_totBytes += actual;
          m_txTrace (packet);
        }
      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed up.
      if ((unsigned)actual != toSend)
        {
          break;
        }
    }
  // Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && m_connected)
    {
      m_socket->Close ();
      m_connected = false;
    }
}

void
PiasBulkSendApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendApplication Connection succeeded");
  m_connected = true;
  SendData ();
}

void
PiasBulkSendApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("BulkSendApplication, Connection Failed");
}

void
PiasBulkSendApplication::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      SendData ();
    }
}

uint8_t 
PiasBulkSendApplication::PiasPriority (uint64_t bytesSent)
{
  NS_LOG_FUNCTION (this << bytesSent);
  if (m_piasPrioNum >= 1)
    {
      uint16_t prioNum = std::min(m_piasPrioNum, (uint16_t)8);
      for (uint8_t i = 0; i < prioNum - 1; i++)
        {
          if (bytesSent <= m_threshs[i])
            return i;
        }
      return prioNum - 1;
    }
  else
      return 0;
}

void
PiasBulkSendApplication::SetPriorityThreshold (uint8_t prio, uint32_t thresh)
{
  NS_LOG_FUNCTION (this << prio << thresh);
  NS_ASSERT_MSG (prio < 8, "Priority must be a value between 0 and 8");
  m_threshs[prio] = thresh;
}
    
} // namespace ns3
