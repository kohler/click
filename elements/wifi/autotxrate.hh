#ifndef CLICK_AUTOTXRATE_HH
#define CLICK_AUTOTXRATE_HH
#include <click/element.hh>
#include <click/dequeue.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * AutoTXRate()
 * 
 * =s wifi
 * 
 * Automatically determine the txrate for a give ethernet dst
 * =over 8
 *
 * =item Probation Count
 * 
 * Unsigned int.  Number of packets before trying the next
 * highest rate.
 *
 * =a
 * SetTXRate, RXStats
 */


class AutoTXRate : public Element { public:
  
  AutoTXRate();
  ~AutoTXRate();
  
  const char *class_name() const		{ return "AutoTXRate"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  AutoTXRate *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String static_print_stats(Element *e, void *);
  String print_stats();

  int get_tx_rate(EtherAddress dst);
 private:
  struct tx_result {
    struct timeval _when;
    int _rate;
    bool _success;
    tx_result(const struct timeval &t, int rate, bool success) : 
      _when(t), 
      _rate(rate), 
      _success(success)
    { }
    tx_result() {}
  };

  
  struct DstInfo {
  public:
    EtherAddress _eth;
    DEQueue<tx_result> _results;
    
    int _rate;


    int _successes;
    int _failures;
    DstInfo() { 
    }

    DstInfo(EtherAddress eth, int rate) { 
      _eth = eth;
      _rate = rate;
    }

  };
  typedef BigHashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  class NeighborTable _neighbors;
  EtherAddress _bcast;
  struct timeval _rate_window;
  void update_rate(EtherAddress dst);
  int set_rate_window(ErrorHandler *, unsigned int);
  static int static_write_rate_window(const String &arg, Element *e,
				      void *, ErrorHandler *errh);
  static int static_write_stepup(const String &arg, Element *e,
				 void *, ErrorHandler *errh);
  static int static_write_stepdown(const String &arg, Element *e,
				   void *, ErrorHandler *errh);
  static int static_write_before_switch(const String &arg, Element *e,
					void *, ErrorHandler *errh);
  static String static_read_rate_window(Element *, void *);
  static String static_read_stepup(Element *, void *);
  static String static_read_stepdown(Element *, void *);
  static String static_read_before_switch(Element *, void *);

  int _stepup;
  int _stepdown;

  int _before_switch;

};

CLICK_ENDDECLS
#endif
