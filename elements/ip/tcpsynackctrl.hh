#ifndef TCPSYNACKCTRL_HH
#define TCPSYNACKCTRL_HH

/*
 * =c
 * TCPSynAckControl()
 * =d
 *
 * Keeps track of half-open connections.
 *
 * Input 0 : SYNs.
 * Input 1 : ACKs.
 *
 * =e
 * = c :: Classifier(33/?2, 331?, -);
 * = t :: TCPSynAckControl();
 * = t[0] -> ...
 * = c[0] -> [0]t;
 * = c[1] -> [1]t;
 *
 * =a
 */

#include "element.hh"
#include "hashmap.hh"
#include "ipflowid.hh"
#include "glue.hh"
#include "click_udp.h"

class TCPSynAckControl : public Element {
public:
  TCPSynAckControl();
  ~TCPSynAckControl();
  
  const char *class_name() const	{ return "TCPSynAckControl"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  TCPSynAckControl *clone() const;
  int configure(const String &, ErrorHandler *);

  void TCPSynAckControl::push(int port_number, Packet *p);


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

    private:
      unsigned int _saddr;
      unsigned int _daddr;

      int _amount;                         // no. of half-open connections

      HalfOpenPorts *_hops[MAX_HALF_OPEN];
      short _free_slots[MAX_HALF_OPEN];  // free slots hop
  };
  
  HashMap<IPFlowID, HalfOpenConnections *> _hoc;
};



#endif
