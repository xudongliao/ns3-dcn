/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "tcp-newreno-d2tcp.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "tcp-socket-base.h"
#include <cmath>



namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpD2TCP");
NS_OBJECT_ENSURE_REGISTERED (TcpD2TCP);

TypeId
TcpD2TCP::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpD2TCP")
    .SetParent<TcpNewReno> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpD2TCP> ()
    .AddAttribute("g",
                  "weight g in D2TCP similar to DCTCP",
                  DoubleValue (1/16),
                  MakeDoubleAccessor (&TcpD2TCP::m_g),
                  MakeDoubleChecker<double> (0));
  return tid;
}

TcpD2TCP::TcpD2TCP () :
 TcpNewReno (),
 m_isCE (false),
 m_hasDelayedACK (false),
 m_highTxMark (0),
 m_bytesAcked (0),
 m_ecnBytesAcked (0),
 m_alpha (0),
 m_g (static_cast<double> (1/16)),
 m_ceFraction (0),
 m_deadlineImminence (0),
 m_penality (0),
 m_timeToAchieve (0),
 m_timeRemain (0),
 m_deadline (0),
 m_deadlineTime (0),
 m_bytesToTx (0),
 m_bytesHasSent (0)
{
  NS_LOG_FUNCTION (this);
}

TcpD2TCP::TcpD2TCP (const TcpD2TCP &sock) :
 TcpNewReno (sock),
 m_isCE (false),
 m_hasDelayedACK (false),
 m_highTxMark (sock.m_highTxMark),
 m_bytesAcked (sock.m_bytesAcked),
 m_ecnBytesAcked (sock.m_ecnBytesAcked),
 m_alpha (sock.m_alpha),
 m_g (static_cast<double> (sock.m_g)),
 m_ceFraction (sock.m_ceFraction),
 m_deadlineImminence (sock.m_deadlineImminence),
 m_penality (sock.m_penality),
 m_timeToAchieve (sock.m_timeToAchieve),
 m_timeRemain (sock.m_timeRemain),
 m_deadline (sock.m_deadline),
 m_deadlineTime (sock.m_deadlineTime),
 m_bytesToTx (sock.m_bytesToTx),
 m_bytesHasSent (sock.m_bytesHasSent)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
}

TcpD2TCP::~TcpD2TCP ()
{
}


std::string
TcpD2TCP::GetName () const
{
  return "D2TCP";
}


void
TcpD2TCP::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
        const Time &rtt, bool withECE, SequenceNumber32 highTxMark, SequenceNumber32 ackNumber)
{
  NS_LOG_DEBUG ("Enter PktsAcked");
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt << withECE << highTxMark << ackNumber);
  m_bytesAcked += segmentsAcked * tcb->m_segmentSize;
  m_bytesHasSent += segmentsAcked * tcb->m_segmentSize;

  if (withECE) {
    m_ecnBytesAcked += segmentsAcked * tcb->m_segmentSize;
  }
  if (ackNumber >= m_highTxMark)
  {
    m_highTxMark = highTxMark;
    m_deadlineTime = tcb->m_deadlineTime;
    m_bytesToTx = tcb->m_bytesToTx;
    UpdateAlpha ();
    UpdateTimeToAcheive (static_cast<double>(tcb->m_cWnd), static_cast<double>(m_bytesToTx - m_bytesHasSent));
    UpdateDeadlineImminence ((m_deadlineTime - Simulator::Now ()).GetSeconds ());
    NS_LOG_LOGIC (this << "deadline remain (s): " << m_timeRemain );
    UpdatePenality ();
    NS_LOG_INFO ("m_alpha: " << m_alpha << " m_penality: " << m_penality << " m_timeToAchieve: " << m_timeToAchieve << " m_timeRemain:" << m_timeRemain);
  }
}


void
TcpD2TCP::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    // In CA_CWR, the D2TCP keeps the windows size like DCTCP
  NS_LOG_DEBUG ("Enter IncreaseWindow ()");
  if (tcb->m_congState != TcpSocketState::CA_CWR)
  {
    TcpNewReno::IncreaseWindow(tcb, segmentsAcked);
  }
}

uint32_t
TcpD2TCP::GetSsThresh (Ptr<TcpSocketState> tcb, uint32_t bytesInFlight) // TODO
{
  NS_LOG_DEBUG ("Enter GetSsThresh, where really calculate SsTresh");
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  if (tcb->m_congState == TcpSocketState::CA_RECOVERY)
  {
      return TcpNewReno::GetSsThresh (tcb, bytesInFlight);
  }
  uint32_t newSsThresh = std::max (static_cast<uint32_t>((1 - m_penality / 2) * tcb->m_cWnd), bytesInFlight / 2);
  NS_LOG_LOGIC (this << Simulator::Now () << "new SsThresh" << newSsThresh);
  return newSsThresh;
}

uint32_t
TcpD2TCP::GetCwnd(Ptr<TcpSocketState> tcb) // TODO
{
  NS_LOG_FUNCTION (this << tcb);
  NS_LOG_DEBUG ("Enter GetCwnd, where really use new cwnd");
  uint32_t newCwnd;
  if (m_penality)
  {
    NS_LOG_DEBUG ("D2TCP, GetCwnd, should not enter here, warning!");
    newCwnd = static_cast<uint32_t>(tcb->m_cWnd + tcb->m_segmentSize);
  }
  else
  {
    newCwnd = static_cast<uint32_t>((1 - m_penality / 2) * tcb->m_cWnd );
  }
  NS_LOG_LOGIC (this << Simulator::Now() << "new cwnd: " << newCwnd );
  return newCwnd;
}



void
TcpD2TCP::CwndEvent(Ptr<TcpSocketState> tcb, TcpCongEvent_t ev, Ptr<TcpSocketBase> socket)
{
  if (ev == TcpCongestionOps::CA_EVENT_ECN_IS_CE && m_isCE == false) // No CE -> CE
  {
    NS_LOG_LOGIC (this << Simulator::Now () << " No CE -> CE ");
    // Note, since the event occurs before writing the data into the buffer,
    // the AckNumber would be the old one, which satisfies our state machine
    if (m_hasDelayedACK)
    {
      NS_LOG_DEBUG ("Delayed ACK exists, sending ACK");
      SendEmptyPacket(socket, TcpHeader::ACK);
    }
    tcb->m_demandCWR = true;
    m_isCE = true;
  }
  else if (ev == TcpCongestionOps::CA_EVENT_ECN_NO_CE && m_isCE == true) // CE -> No CE
  {
    NS_LOG_LOGIC (this << " CE -> No CE ");
    if (m_hasDelayedACK)
    {
      NS_LOG_DEBUG ("Delayed ACK exists, sending ACK | ECE");
      SendEmptyPacket(socket, TcpHeader::ACK | TcpHeader::ECE);
    }
    tcb->m_demandCWR = false;
    m_isCE = false;
  }
  else if (ev == TcpCongestionOps::CA_EVENT_DELAY_ACK_RESERVED)
  {
    m_hasDelayedACK = true;
    NS_LOG_LOGIC (this << " Reserve deplay ACK ");
  }
  else if (ev == TcpCongestionOps::CA_EVENT_DELAY_ACK_NO_RESERVED)
  {
    m_hasDelayedACK = false;
    NS_LOG_LOGIC (this << " Cancel deplay ACK ");
  }
}

Ptr<TcpCongestionOps>
TcpD2TCP::Fork ()
{
  return CopyObject<TcpD2TCP> (this);
}

void
TcpD2TCP::UpdateTimeToAcheive (double windowSize, double remainingBytes)
{
  m_timeToAchieve = static_cast<double>(remainingBytes) / static_cast<double>(3.0 / 4 * windowSize);
}
void
TcpD2TCP::UpdateDeadlineImminence (double timeRemain)
{
  // double windowSize = 
  // remainingBytes
  // UpdateTimeToAcheive (windowSize, remainingBytes);
  m_timeRemain = timeRemain;
  m_deadlineImminence = m_timeToAchieve / m_timeRemain;
}

void
TcpD2TCP::UpdateAlpha ()
{
  if (m_bytesAcked == 0)
    {
      m_ceFraction = 0.0;
    }
  else
    {
      m_ceFraction = static_cast<double>(m_ecnBytesAcked) / static_cast<double>(m_bytesAcked);
      NS_LOG_INFO ("m_ecnBytesAcked: " << m_ecnBytesAcked << "  m_ceFraction: " << m_ceFraction);
    }
  m_alpha = (1 - m_g) * m_alpha + m_g * m_ceFraction;
  // Ns_LOG_DEBUG ("m_ceFraction: " << m_ceFraction << " m_g: " << m_g);
  // NS_LOG_DEBUG ("m_ceFraction: " << m_ceFraction);
  // Ns_LOG_LOGIC (this << Simulator::Now() << " alpha updated: " << m_alpha << 
  //             " and ECN fraction: " << m_ceFraction <<
  //             " bytes acked: " << m_bytesAcked <<
  //             " ecn bytes acked: " << m_ecnBytesAcked);
  // refresh
  m_bytesAcked = 0;
  m_ecnBytesAcked = 0;
}

void
TcpD2TCP::UpdatePenality ()
{
  m_penality = std::pow(m_alpha, m_deadlineImminence);
}

} // namespace ns3

