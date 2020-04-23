#ifndef PIAS_BULK_SEND_APPLICATION_H
#define PIAS_BULK_SEND_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include <array>

namespace ns3{

class Socket;
class Packet;
class Address;

#define MEAN_PACKET_SIZE 1460

typedef std::array<uint32_t, 7> PiasThreshold;

class PiasBulkSendApplication : public Application
{
public:
  
  static TypeId GetTypeId(void);

  PiasBulkSendApplication();

  virtual ~PiasBulkSendApplication();

  /**
   * \brief Set upper bound for the total amount of data to send.
   * 
   * Once this bound is reached, no more application bytes are sent. If the
   * application is stopped during the simulation and restarted, the
   * total number of bytes sent is not reset; however, the maxBytes
   * bound is still effective and the application will continue sending
   * up to maxBytes. The value zero for maxBytes means that
   * there is no upper bound; i.e. data is sent until the application
   * or simulation is stopped.
   * 
   * \param maxBytes the upper bound of bytes to send
   */
  void SetMaxBytes(uint32_t maxBytes);

  /**
   * \brief Get the socket this application is attached to.
   * \return pointer to associated socket
   */
  Ptr<Socket> GetSocket (void) const; 

  /**
   * \brief Set threshold for each priority
   * \param the priority level
   * \param the correspoding threshold
   */
  void SetPriorityThreshold (uint8_t prio, uint32_t thresh);

protected:
  virtual void DoDispose (void);

private:
  // inherent from base class
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  /**
   * \brief Send data until the L4 transmission buffer is full.
   */
  void SendData ();

  Ptr<Socket> m_socket; //!< Associated socket
  Address m_peer;       //!< Peer address
  bool m_connected;     //!< True if connected
  uint32_t m_sendSize;  //!< Size of data to send each time
  uint64_t m_maxBytes;  //!< Limit total number of bytes sent
  uint64_t m_totBytes;  //!< Total bytes sent so far
  TypeId m_tid;         //!< The type of protocol to use.

  bool m_isDelay;
  Time m_delayTime;
  uint32_t m_accumPackets;
  uint32_t m_delayThresh;

  uint32_t m_tos;
  uint32_t m_flowid;

  // Traced Callback: sent packets
  TracedCallback<Ptr<const Packet>> m_txTrace;

  uint16_t m_piasPrioNum;     //!< Number of PIAS priorities
  PiasThreshold m_threshs;   //!< PIAS priorities threshold

private:
  /**
   * \brief Connection Succeeded (called by Socket through a callback)
   * \param socket the connected socket
   */
  void ConnectionSucceeded (Ptr<Socket> socket);

  /**
   * \brief Connection Failed (called by Socket through a callback)
   * \param socket the connected socket
   */
  void ConnectionFailed (Ptr<Socket> socket);

  /**
   * \brief Send more data as soon as some has been transmitted.
   */
  void DataSend (Ptr<Socket>, uint32_t); // for socket's SetSendCallback

  void ResumeSend (void);
  /**
   * \brief Calculate PIAS Priority based on the sent size
   * \param bytesSent Total bytes sent so far
   */
  uint8_t PiasPriority (uint64_t bytesSent);
};

/**
 * Serialize the priomap to the given ostream
 *
 * \param os
 * \param priomap
 *
 * \return std::ostream
 */
std::ostream &operator << (std::ostream &os, const PiasThreshold &threshs);

/**
 * Serialize from the given istream to this priomap.
 *
 * \param is
 * \param priomap
 *
 * \return std::istream
 */
std::istream &operator >> (std::istream &is, PiasThreshold &threshs);

ATTRIBUTE_HELPER_HEADER (PiasThreshold);
} // namespace ns3

#endif /* PIAS_BULK_SEND_APPLICATION_H */
