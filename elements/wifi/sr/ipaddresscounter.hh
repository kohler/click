#ifndef IPADDRESSCOUNTER_HH
#define IPADDRESSCOUNTER_HH

/*
=c

IPAddressCounter()

=s Wifi, Wireless Routing

Count traffic for individual ip addresses.

=d

Expects a SR MAC packet as input.
Calculates the SR header's checksum and sets the version and checksum header fields.

=a
CheckSRHeader */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/bighashmap.hh>
CLICK_DECLS

class IPAddressCounter : public Element {
public:
  IPAddressCounter();
  ~IPAddressCounter();
  
  const char *class_name() const		{ return "IPAddressCounter"; }
  const char *processing() const		{ return AGNOSTIC; }

  static int write_param(const String &arg, Element *e,
				void *, ErrorHandler *errh); 

  static String read_param(Element *e, void *);


  String stats();
  void add_handlers();

  int configure (Vector<String> &conf, ErrorHandler *errh);

  class IPAddressInfo {
  public:
    IPAddressInfo() { 
      _packets = 0;
      _bytes = 0;
    }
    IPAddressInfo(IPAddress ip) {
      _ip = ip;
      _packets = 0;
      _bytes = 0;
    }
    IPAddress _ip;
    int _packets;
    int _bytes;
    struct timeval _last_packet;
  };

  typedef HashMap<IPAddress, IPAddressInfo> IPInfoTable;
  typedef IPInfoTable::const_iterator IPIter;


  class IPInfoTable _table;

  bool _track_src;
  bool _track_dst;

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
