#ifndef CLICK_WIFIDUPEFILTER_HH
#define CLICK_WIFIDUPEFILTER_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/hashmap.hh>
CLICK_DECLS

/*
=c

WifiDupeFilter([TAG] [, KEYWORDS])

=s Wifi

Filters out duplicate 802.11 packets based on their sequence number.

=d

=a WifiEncap, WifiDecap
 */

class WifiDupeFilter : public Element {

 public:

  WifiDupeFilter() CLICK_COLD;
  ~WifiDupeFilter() CLICK_COLD;

  const char *class_name() const		{ return "WifiDupeFilter"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

  static String static_read_stats(Element *xf, void *);
  static String static_read_debug(Element *xf, void *);
  static int static_write_debug(const String &arg, Element *e,
				void *, ErrorHandler *errh);
  void add_handlers() CLICK_COLD;

  class DstInfo {
  public:
    EtherAddress _eth;
    int _dupes;
    int _packets;

    uint16_t seq;
    uint16_t frag;

    DstInfo(EtherAddress eth) {
      _eth = eth;
    }
    DstInfo () { }
    void clear() {
      _dupes = 0;
      _packets = 0;
      seq = 0;
      frag = 0;

    }
  };

  typedef HashMap <EtherAddress, DstInfo> DstTable;
  typedef DstTable::const_iterator DstIter;

  DstTable _table;
  bool _debug;

  int _dupes;
};

CLICK_ENDDECLS
#endif
