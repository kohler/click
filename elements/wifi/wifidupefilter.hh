#ifndef CLICK_WIFIDUPEFILTER_HH
#define CLICK_WIFIDUPEFILTER_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/dequeue.hh>
#include <click/hashmap.hh>
CLICK_DECLS

/*
 * =c
 * WifiDupeFilter([TAG] [, KEYWORDS])
 * =s debugging
 * =d
 * Assumes input packets are SR packets (ie a sr_pkt struct from 
 * sr.hh). Prints out a description of those packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =a
 * Print, SR
 */

class WifiDupeFilter : public Element {
  
 public:
  
  WifiDupeFilter();
  ~WifiDupeFilter();
  
  const char *class_name() const		{ return "WifiDupeFilter"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

  static String static_read_stats(Element *xf, void *);
  static String static_read_debug(Element *xf, void *);
  static int static_write_debug(const String &arg, Element *e,
				void *, ErrorHandler *errh);
  void add_handlers();

  class DstInfo {
  public:
    EtherAddress _eth;

    struct timeval _last;
    int _dupes;
    int _packets;
    DEQueue<int> _sequences; //most recently received seq nos
    DstInfo(EtherAddress eth) {
      _eth = eth;
    }
    DstInfo () { }
    void clear() {
      _dupes = 0;
      _packets = 0;
      _sequences.clear();
      click_gettimeofday(&_last);
    }
  };

  typedef HashMap <EtherAddress, DstInfo> DstTable;
  typedef DstTable::const_iterator DstIter;

  DstTable _table;
  int _window;
  bool _debug;

  int _dupes;
  int _packets;
};

CLICK_ENDDECLS
#endif
