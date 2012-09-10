#ifndef CLICK_WIFIDEFRAG_HH
#define CLICK_WIFIDEFRAG_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
CLICK_DECLS

/*
=c

WifiDefrag

=s Wifi

Reassembles 802.11 fragments.

=d

This element reassembles 802.11 fragments and pushes the reassembled
packet out when the last fragment is received. It does not affect
packets that are not fragmented, and passes them through its output
unmodified.  In accordance with the 802.11 spec, it supports a single
packet per SA.

=a WifiDecap
 */
class WifiDefrag : public Element { public:

  WifiDefrag() CLICK_COLD;
  ~WifiDefrag() CLICK_COLD;

  const char *class_name() const	{ return "WifiDefrag"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers() CLICK_COLD;


  struct PacketInfo {
    EtherAddress src;
    uint16_t next_frag;
    uint16_t seq;
    Packet *p;

    PacketInfo() {
      p = 0;
      clear();
    }
    PacketInfo(EtherAddress s) {
      p = 0;
      clear();
      src = s;
    }

    void clear() {
      if (p) {
	p->kill();
      }
      p = 0;
      next_frag = 0;
      seq = 0;
    }
  };


  typedef HashMap<EtherAddress, PacketInfo> PacketInfoTable;
  typedef PacketInfoTable::const_iterator PIIter;

  PacketInfoTable _packets;
  bool _debug;
  static String read_param(Element *e, void *thunk) CLICK_COLD;
  static int write_param(const String &in_s, Element *e, void *vparam,
			 ErrorHandler *errh);
 private:

};

CLICK_ENDDECLS
#endif
