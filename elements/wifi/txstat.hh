#ifndef TXSTATHH
#define TXSTATHH

/*
 * =c
 * TXStat([I<KEYWORDS>])
 * =s Grid
 * Track tx stats
 *
 * =d
 *
 * blah blah blah.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item ETH, IP
 *
 * Ethernet and IP addresses of this node, respectively; required if
 * output is connected.
 *
 *
 * =back
 *
 */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <elements/grid/timeutils.hh>
#include <elements/grid/grid.hh>

CLICK_DECLS

class TXStat : public Element {
private:

  class TXNeighborInfo {
  public:
    EtherAddress _eth;
    int _long_retries;
    int _short_retries;
    int _failures;
    int _rate;
    int _packets_sent;
    TXNeighborInfo() { 
      _eth = EtherAddress(); 
      reset();
    }
    TXNeighborInfo(EtherAddress eth) {  
      _eth = eth; 
      reset();
    }

    void reset() {
      _long_retries = 0; 
      _short_retries = 0; 
      _failures = 0;
      _rate = 0; 
      _packets_sent = 0;

    }
  };
  typedef BigHashMap<EtherAddress, TXNeighborInfo> TXNeighborTable;
  typedef TXNeighborTable::const_iterator TXNIter;


  class  TXNeighborTable _neighbors;

  EtherAddress _eth;
  EtherAddress _bcast;


public:
  
  TXStat();
  ~TXStat();
  
  const char *class_name() const		{ return "TXStat"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const                 { return "x/y"; }
  void notify_noutputs(int);
  
  TXStat *clone() const;

  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);


  static String static_print_tx_stats(Element *e, void *);
  String print_tx_stats();
};

CLICK_ENDDECLS
#endif
