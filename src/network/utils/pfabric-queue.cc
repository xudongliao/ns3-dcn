/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#include "ns3/log.h"
#include "pfabric-queue.h"
#include "ns3/socket.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PfabricQueue");

NS_OBJECT_ENSURE_REGISTERED (PfabricQueue);

TypeId PfabricQueue::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PfabricQueue")
    .SetParent<Queue> ()
    .SetGroupName ("Network")
    .AddConstructor<PfabricQueue> ()
  ;
  return tid;
}

PfabricQueue::PfabricQueue () :
  Queue (),
  m_packets ()
{
  NS_LOG_FUNCTION (this);
}

PfabricQueue::~PfabricQueue ()
{
  NS_LOG_FUNCTION (this);
}

bool 
PfabricQueue::DoEnqueue (Ptr<QueueItem> item)
{
  NS_LOG_FUNCTION (this << item);
  NS_ASSERT (m_packets.size () == GetNPackets ());

  m_packets.push_back (item);

  return true;
}

Ptr<QueueItem>
PfabricQueue::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_packets.size () == GetNPackets ());
  int shortest_flow_id = 0, min_value = 1<<30;
  int size = m_packets.size();

  for(int i = 0 ; i < size ; ++i)
  {
      Ptr<QueueItem> _item = m_packets[i];
      SocketPriorityTag priorityTag;
      SocketFlowidTag flowidTag;
      if(!(_item->GetPacket ()->PeekPacketTag (priorityTag))||!(_item->GetPacket ()->PeekPacketTag (flowidTag))) //dequeue the control packet first
      {
        Ptr<QueueItem> item = m_packets[i];
        m_packets.erase (m_packets.begin()+i);
        NS_LOG_LOGIC ("Popped " << item);
        return item;
      }
      int prio = priorityTag.GetPriority();
      int flowid = flowidTag.GetFlowid();
      if(prio < min_value)
      {
        shortest_flow_id = flowid;
        min_value = prio;
      }
  }
  int dequeue_id = 0;
  for(int i = 0 ; i < size ; ++i)
  {
      Ptr<QueueItem> _item = m_packets[i];
      SocketFlowidTag flowidTag;
      NS_ASSERT(_item->GetPacket ()->PeekPacketTag (flowidTag));
      int flowid = flowidTag.GetFlowid();
      if(flowid == shortest_flow_id)
      {
        dequeue_id = i;
        break;
      }
  }

  Ptr<QueueItem> item = m_packets[dequeue_id];
  m_packets.erase (m_packets.begin()+dequeue_id);

  NS_LOG_LOGIC ("Popped " << item);

  return item;
}

Ptr<const QueueItem>
PfabricQueue::DoPeek (void) const
{
  
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_packets.size () == GetNPackets ());
  int shortest_flow_id = 0, min_value = 1<<30;
  int size = m_packets.size();

  for(int i = 0 ; i < size ; ++i)
  {
      Ptr<QueueItem> _item = m_packets[i];
      SocketPriorityTag priorityTag;
      SocketFlowidTag flowidTag;
      if(!(_item->GetPacket ()->PeekPacketTag (priorityTag))||!(_item->GetPacket ()->PeekPacketTag (flowidTag))) //dequeue the control packet first
      {
        Ptr<QueueItem> item = m_packets[i];
        return item;
      }
      int prio = priorityTag.GetPriority();
      int flowid = flowidTag.GetFlowid();
      if(prio < min_value)
      {
        shortest_flow_id = flowid;
        min_value = prio;
      }
  }
  int dequeue_id = 0;
  for(int i = 0 ; i < size ; ++i)
  {
      Ptr<QueueItem> _item = m_packets[i];
      SocketFlowidTag flowidTag;
      NS_ASSERT(_item->GetPacket ()->PeekPacketTag (flowidTag));
      int flowid = flowidTag.GetFlowid();
      if(flowid == shortest_flow_id)
      {
        dequeue_id = i;
        break;
      }
  }

  Ptr<QueueItem> item = m_packets[dequeue_id];
  return item;
}

} // namespace ns3
