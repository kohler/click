#ifndef CLICK_WIFICOUNTER_HH
#define CLICK_WIFICOUNTER_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * WifiCounter()
 * 
 * =s wifi
 * 
 * Accumulate wificounter for each ethernet src you hear a packet from.
 * =over 8
 *
 *
 */


class WifiCounter : public Element { public:
  
  WifiCounter();
  ~WifiCounter();
  
  const char *class_name() const		{ return "WifiCounter"; }
  const char *processing() const		{ return PORTS_1_1; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action (Packet *p_in);

  void add_handlers();

  String stats();
  class WifiPacketCount {
  public:
    int count;
    int bytes;
    int tx_time;

    WifiPacketCount() { 
      count = 0;
      bytes = 0;
      tx_time = 0;
    }

    String s() {
      return String(count) + " " +
	String(bytes) + " " +
	String(tx_time) + " ";
    }
  };
  class DstInfo {
  public:
    EtherAddress eth;
    WifiPacketCount types[4][16];

    WifiPacketCount totals;

    DstInfo() { }
    DstInfo(EtherAddress e) { eth = e; } 
  };

  WifiPacketCount types[4][16];
  WifiPacketCount totals;

  typedef HashMap<EtherAddress, DstInfo> EtherTable;
  typedef EtherTable::const_iterator ETIter;
  
  EtherTable _dsts;

};

CLICK_ENDDECLS
#endif
