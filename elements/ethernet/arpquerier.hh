#ifndef ARPQUERIER_HH
#define ARPQUERIER_HH

/*
 * =c
 * ARPQuerier(I, E)
 * =d
 * Handles most of the ARP protocol. Argument I should be
 * this host's IP address, and E should be this host's
 * ethernet address.
 *
 * Expects ordinary IP packets on input 0, each with a destination
 * address annotation. If an ethernet address is already known
 * for the destination, the IP packet is wrapped in an ethernet
 * header and sent to output 0. Otherwise the IP packet is saved and
 * an ARP query is sent to output 0. If an ARP response arrives
 * on input 1 for an IP address that we need, the mapping is
 * recorded and the saved IP packet is sent.
 *
 * The ARP reply packets on input 1 should include the ethernet header.
 *
 * If a host has multiple interfaces, it will need multiple
 * instances of ARPQuerier.
 *
 * =e
<pre>
c :: Classifier(12/0806 20/0001, 12/0800, ...);
a :: ARPQuerier(18.26.4.24, 00:00:C0:AE:67:EF);
c[0] -> a[1];
c[1] -> ... -> a[0];
a[0] -> ... -> KernelWriter(eth0);
</pre>
 *
 * =a ARPResponder
 * =a ARPFaker
 */

#include "element.hh"
#include "etheraddress.hh"
#include "ipaddress.hh"
#include "hashmap.hh"

class ARPQuerier : public Element {
 public:
  
  ARPQuerier();
  ~ARPQuerier();
  
  const char *class_name() const		{ return "ARPQuerier"; }
  Processing default_processing() const	{ return PUSH; }
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  void add_handlers(HandlerRegistry *fcr);
  
  ARPQuerier *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  
  void push(int port, Packet *);
  
  Packet *make_query(unsigned char tpa[4],
                     unsigned char sha[6], unsigned char spa[4]);

  void insert(IPAddress, EtherAddress);

  struct ARPEntry {
    EtherAddress a;
    int ok;              // Is the EtherAddress valid?
    struct timeval when; // Time of last response heard.
    int polling;         // Refreshing the ARP entry.
    Packet *p;
  };

  bool each(int &i, IPAddress &k, ARPEntry &v) const;
  
 private:
  
  HashMap<IPAddress, ARPEntry *> _map;
  EtherAddress _my_en;
  IPAddress _my_ip;
  
  Packet *lookup(Packet *p);
  Packet *query_for(Packet *p);
  Packet *response(Packet *p);
  
};

#endif
