#ifndef TCPCONNECTIONMONITOR_HH
#define TCPCONNECTIONMONITOR_HH

/*
 * =c
 * TCPConnectionMonitor(THRESH)
 * =d
 *
 * Keeps track of half-open connections.
 *
 * Input 0 : SYNs.
 * Input 1 : ACKs.
 *
 * =e
 * = c :: Classifier(33/12, 33/?2, 33/1?, -);
 * = t :: TCPConnectionMonitor(15);
 * = t[0] -> ...
 * = c[0] -> ...        // SYN/ACK
 * = c[1] -> [0]t;      // SYN
 * = c[2] -> [1]t;      // ACK
 * = c[3] -> ...        // other
 *
 * =a
 */

#include "element.hh"
#include "hashmap.hh"
#include "ipflowid.hh"
#include "glue.hh"
#include "click_udp.h"

class TCPConnectionMonitor : public Element {
public:
  TCPConnectionMonitor();
  ~TCPConnectionMonitor();
  
  const char *class_name() const	{ return "TCPConnectionMonitor"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  TCPConnectionMonitor *clone() const;
  int configure(const String &, ErrorHandler *);

  void TCPConnectionMonitor::push(int port_number, Packet *p);


private:
#define MAX_HALF_OPEN   32

  struct HalfOpenPorts {
    unsigned short sport;
    unsigned short dport;
  };



  // Maintains a list of all half-open connections between a specific source IP
  // address and dest. IP address.
  class HalfOpenConnections {
    public:
      HalfOpenConnections(IPAddress, IPAddress);
      ~HalfOpenConnections();
      void add(unsigned short, unsigned short);
      void del(unsigned short, unsigned short);
      int amount() { return _amount; }
      bool half_open_ports(int i, HalfOpenPorts &hops);

    private:
      unsigned int _saddr;
      unsigned int _daddr;
      int _amount;                         // no. of half-open connections

      // Ports associated with this src and dst
      HalfOpenPorts *_hops[MAX_HALF_OPEN];
      short _free_slots[MAX_HALF_OPEN];
  };
  
  HashMap<IPFlowID, HalfOpenConnections *> _hoc;
  int _thresh;

  void add_handlers();
  static String thresh_read_handler(Element *e, void *);
  static int thresh_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh);
  static String look_read_handler(Element *e, void *);
};



#endif // TCPCONNECTIONMONITOR_HH
