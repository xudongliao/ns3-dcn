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
#ifndef TCP_NEWRENO_D2TCP_H
#define TCP_NEWRENO_D2TCP_H

#include "tcp-congestion-ops.h"

namespace ns3 {

class TcpD2TCP : public TcpNewReno
{
public:
  /**
   * \brief Get the type ID.
   * \return the pbject TypeID
   */
  static TypeId GetTypeId (void);

  TcpD2TCP ();
  TcpD2TCP (const TcpD2TCP &sock);

  ~TcpD2TCP ();

  std::string GetName() const;

  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual uint32_t GetSsThresh (Ptr<TcpSocketState> tcb,
                                uint32_t bytesInFlight);
  // virtual void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual uint32_t GetCwnd(Ptr<TcpSocketState> tcb);
  virtual void CwndEvent(Ptr<TcpSocketState> tcb, TcpCongEvent_t ev, Ptr<TcpSocketBase> socket);
  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                          const Time& rtt, bool withECE,
                          SequenceNumber32 highTxMark, SequenceNumber32 ackNumber) { }
  virtual Ptr<TcpCongestionOps> Fork ();

  /**
   * \brief calculate and set m_timeToAchieve
   * \param W: flow's current window size, B: B bytes remaining to transmit for this flow.
   * \return none
   */
  void UpdateTimeToAcheive (double windowSize, double remainingBytes) ;
  /**
   * \brief calculate and set m_d
   * \return none
   */
  void UpdateDeadlineImminence ();

  void UpdateAlpha ();
  void UpdatePenality ();

protected:
  double m_bytesAcked;
  double m_ecnBytesAcked;

  double m_alpha; // alpha = (1 - g) * alpha + g * f
                  // a weighted average quantitatively measures the extent of congestion
  double m_g;     // default, 1/16
  double m_ceFraction;    // the fraction of packets marked with CE bits in the most recent window
  double m_deadlineImminence; // d: the deadline imminence, reference to pape, a larger d implies a closer deadline.
  double m_penality;      // the penalty function: p = alpha ^ d

  double m_timeToAchieve;    // T_c: the time needed for a flow to complete transmitting all its data under dealine-agnostic behavior
  double m_timeRemain;       // D: the time remaining until its deadline expires.

};




}// namespace ns3
#endif /* TCP_NEWRENO_D2TCP_H */