#ifndef DHCPSERVER_HH
#define DHCPSERVER_HH

#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include <click/dequeue.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/timer.hh>
#include <click/timestamp.hh>

/*
 * =c
 * DHCPServer( ServerIPAddress, SubnetMask )
 *
 * =s 
 * The core of the DHCP Server. Responsible of keeping track of
 * free and allocated leases
 *
 * =d 
 * 
 * DHCPServer is responsible of keeping track of free,
 * reservered, and allocated leases. 
 *   
 * =e
 * DHCPServer(192.168.10.9, 192.168.10.0);
 *
 * =a
 * DHCPServerOffer, DHCPServerACKorNACK, DHCPServerRelease
 *
 */


class Lease {
 public:
	Lease() { }
	~Lease() { }
	
	EtherAddress _eth;
	IPAddress _ip;
	Timestamp _start;
	Timestamp _end;
	Timestamp _duration;
	bool _valid;
	void extend() {
		Timestamp now = Timestamp::now();
		_start.set_sec(now.sec());
		_start.set_subsec(now.subsec());
		_end.set_sec( _start.sec() + now.sec() );
		_end.set_subsec( _start.subsec() + now.subsec() );
	}
};

class DHCPServer : public Element {
public:
  DHCPServer();
  ~DHCPServer();
  const char* class_name() const { return "DHCPServer"; }
  const char* processing() const { return AGNOSTIC; }
  const char* port_count() const { return PORTS_1_1; }
  const char* flow_code()  const { return "x/y"; }
  void* cast(const char*);
  int configure( Vector<String> &conf, ErrorHandler *errh );
  void add_handlers();
  virtual Packet *simple_action(Packet *);
  
  void remove(IPAddress ip);
  void remove(EtherAddress eth);
  Lease *new_lease(EtherAddress, IPAddress);
  Lease *new_lease_any(EtherAddress);
  IPAddress get_server_ip();
  void insert(Lease);


  Packet *make_ack_packet(Packet *, Lease *);
  Packet *make_nak_packet(Packet *, Lease *);
  Packet *make_offer_packet(Packet *, Lease *);




private:
  static String read_handler(Element *, void *);
  Lease *rev_lookup(EtherAddress eth);
  Lease *lookup(IPAddress ip);

  typedef HashMap<IPAddress, Lease> LeaseMap;
  typedef LeaseMap::const_iterator LeaseIter;
  LeaseMap _leases;

  HashMap<EtherAddress, IPAddress> _ips;

  DEQueue<IPAddress> _free_list;
  HashMap<IPAddress, IPAddress> _free;

  IPAddress _start;
  IPAddress _end;

  IPAddress _ip;

  bool _debug;

  IPAddress _bcast;
};

#endif /* DHCPSERVER_HH */
