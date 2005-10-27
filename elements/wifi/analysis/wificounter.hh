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

class EtherPair {
 public: 
	EtherAddress _src;
	EtherAddress _dst;
	EtherPair () { }
	EtherPair(EtherAddress src, EtherAddress dst) {
		_src = src;
		_dst = dst;
	}
	inline bool operator==(EtherPair other) {
		return _src == other._src && _dst == other._dst;
	}
};


inline unsigned hashcode(EtherPair p) {
	return hashcode(p._src) + hashcode(p._dst);
}

class WifiCounter : public Element { public:
  
  WifiCounter();
  ~WifiCounter();
  
  const char *class_name() const		{ return "WifiCounter"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action (Packet *p_in);

  void add_handlers();

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

  class EtherPairCount {
  public:
	  EtherPair _pair;
	  int _count;
	  EtherPairCount() {_count = 0; }
	  EtherPairCount(EtherPair p) {
		  _pair = p;
		  _count = 0;
	  }
	  inline bool operator==(EtherPairCount other) {
		  return _pair == other._pair;
	  }
  };
  typedef HashMap<EtherPair, EtherPairCount> EtherTable;
  typedef EtherTable::const_iterator ETIter;
  
  EtherTable _pairs;

};

CLICK_ENDDECLS
#endif
