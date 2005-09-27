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
#include <elements/wifi/linktable.hh>
#include <elements/wifi/arptable.hh>
#include <elements/wifi/sr/path.hh>
#include <click/ipflowid.hh>
#include <clicknet/tcp.h>
#include "srpacket.hh"
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
  const char *port_count() const		{ return "2/2"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/#"; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();
  static int change_param(const String &in_s, Element *e, void *thunk, ErrorHandler *errh);
  static String read_param(Element *e, void *vparam);
  void push(int, Packet *);
  void run_timer(Timer *);
  static String static_print_flows(Element *, void *);
private:

  class GatewaySelector *_gw_sel;
  Timer _timer;

  class FlowTableEntry {
  public:
    class IPFlowID _id;
    IPAddress _gw;
    Timestamp _oldest_unanswered;
    Timestamp _last_reply;
    int _outstanding_syns;
    bool _fwd_alive;
    bool _rev_alive;
    bool _all_answered;
    FlowTableEntry() {
      _all_answered = true;
      _fwd_alive = true;
      _rev_alive = true;
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
	  _oldest_unanswered = Timestamp::now();
	  _all_answered = false;
      }
    }
    void saw_reply_packet() {
      _last_reply = Timestamp::now();
      _all_answered = true;
      _outstanding_syns = 0;
    }
    bool is_pending() const    { return (_outstanding_syns > 0);}

    Timestamp age() {
	    return Timestamp::now() - _last_reply;
    }
  };

  typedef HashMap<IPFlowID, FlowTableEntry> FlowTable;
  typedef FlowTable::const_iterator FTIter;
  FlowTable _flow_table;

  IPAddress _gw;
  void push_fwd(Packet *, IPAddress);
  void push_rev(Packet *);
  void cleanup();
  String print_flows();
};


CLICK_ENDDECLS
#endif
