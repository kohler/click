#ifndef CLICK_SETGATEWAY_HH
#define CLICK_SETGATEWAY_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/dequeue.hh>
#include <elements/grid/linktable.hh>
#include <elements/grid/arptable.hh>
#include <elements/grid/sr/path.hh>
#include <click/ipflowid.hh>
#include <clicknet/tcp.h>
#include "srcr.hh"
#include "gatewayselector.hh"
#include <elements/wifi/rxstats.hh>
CLICK_DECLS

/*
 * =c
 * SetGateway([GW ipaddress], [SEL GatewaySelector element])
 * =d
 * 
 * This element marks the gateway for a packet to be sent to.
 * Either manually specifiy an gw using the GW keyword
 * or automatically select it using a GatewaySelector element.
 * 
 *
 */


class SetGateway : public Element {
 public:
  
  SetGateway();
  ~SetGateway();
  
  const char *class_name() const		{ return "SetGateway"; }
  const char *processing() const		{ return PUSH; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static int change_param(const String &in_s, Element *e, void *thunk, ErrorHandler *errh);
  static String read_param(Element *e, void *vparam);
  void push(int, Packet *);
  void run_timer();
  static String static_print_flows(Element *, void *);
private:

  class GatewaySelector *_gw_sel;
  Timer _timer;

  class FlowTableEntry {
  public:
    class IPFlowID _id;
    IPAddress _gw;
    struct timeval _oldest_unanswered;
    struct timeval _last_reply;
    int _outstanding_syns;
    bool _fwd_alive;
    bool _rev_alive;
    bool _all_answered;
    FlowTableEntry() {
      _all_answered = true;
    }

    FlowTableEntry(const FlowTableEntry &e) : 
      _id(e._id),
      _gw(e._gw),
      _oldest_unanswered(e._oldest_unanswered),
      _last_reply(e._last_reply),
      _outstanding_syns(e._outstanding_syns),
      _fwd_alive(e._fwd_alive),
      _rev_alive(e._rev_alive),
      _all_answered(e._all_answered)
    { }

    void saw_forward_packet() {
      if (_all_answered) {
	click_gettimeofday(&_oldest_unanswered);
	_all_answered = false;
      }
    }
    void saw_reply_packet() {
      click_gettimeofday(&_last_reply);
      _all_answered = true;
    }
    bool is_pending() const    { return (_outstanding_syns > 0);}

    struct timeval age() {
      struct timeval age;
      struct timeval now;
      timerclear(&age);
      if (_fwd_alive || _rev_alive) {
	return age;
      }
      click_gettimeofday(&now);
      timersub(&now, &_last_reply, &age);
      return age;
    }
  };

  typedef HashMap<IPFlowID, FlowTableEntry> FlowTable;
  typedef FlowTable::const_iterator FTIter;
  class FlowTable _flow_table;

  IPAddress _gw;
  void push_fwd(Packet *, IPAddress);
  void push_rev(Packet *);
  void cleanup();
  String print_flows();
};


CLICK_ENDDECLS
#endif
