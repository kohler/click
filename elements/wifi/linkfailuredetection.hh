#ifndef CLICK_LINKFAILUREDETECTION_HH
#define CLICK_LINKFAILUREDETECTION_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * LinkFailureDetection(THRESHOLD, HANDLER)
 * 
 * Takes ethernet packets and checks if the wifi tx_success
 * is set to failure.  
 * after THRESHOLD failures for a particular ethernet destination,
 * it will call HANDLER.
 * A successful transmission resets the failure count to 0.
 * 
 * Regular Arguments:
 * =over 8
 *
 * =item THRESHOLD
 *
 * Unsigned integer. Number of failures before HANLDER is called
 *
 *
 * =item  HANDLER
 *
 * The write handler to call when a link failure is detected.
 * This handler will be called with a string that is the ethernet
 * mac address of the failed destination.
 * =back
 * 
 *
 * =s wifi
 *
 *
 */


class LinkFailureDetection : public Element { public:
  
  LinkFailureDetection();
  ~LinkFailureDetection();
  
  const char *class_name() const		{ return "LinkFailureDetection"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String static_print_stats(Element *e, void *);
  String print_stats();

  int get_rate(EtherAddress);
 private:

  
  class DstInfo {
  public:
    EtherAddress _eth;
    int _successive_failures;
    struct timeval _last_received;
    bool _notified;
    DstInfo() { 
      memset(this, 0, sizeof(*this));
    }

    DstInfo(EtherAddress eth) { 
      memset(this, 0, sizeof(*this));
      _eth = eth;
      _successive_failures = 0;
      _notified = false;
    }

  };

  int _threshold;
  Element *_handler_e;
  String _handler_name;
  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  class NeighborTable _neighbors;
  EtherAddress _bcast;
  int _tau;


  void call_handler(EtherAddress);
};

CLICK_ENDDECLS
#endif
