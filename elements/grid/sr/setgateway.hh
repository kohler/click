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
 * SetGateway(IP, ETH, ETHERTYPE, LinkTable, ARPTable, 
 *                 [PERIOD timeout], [GW is_gateway], 
 *                 [METRIC GridGenericMetric])
 * =d

 * Input 0: packets from dev
 * Input 1: packets for gateway node
 * Output 0: packets to dev
 * Output 1: packets with dst_ip anno set
 *
 * This element provides proactive gateway selection.  
 * Each gateway broadcasts an ad every PERIOD seconds.  
 * Non-gateway nodes select the gateway with the best metric
 * and forward ads.
 * 
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
  SetGateway *clone() const;
  int configure(Vector<String> &conf, ErrorHandler *errh);


  /* handler stuff */
  void add_handlers();

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

  typedef BigHashMap<IPFlowID, FlowTableEntry> FlowTable;
  typedef FlowTable::const_iterator FTIter;
  class FlowTable _flow_table;

  void push_fwd(Packet *, IPAddress);
  void push_rev(Packet *);
  void cleanup();
  void setgateway_assert_(const char *, int, const char *) const;
  String print_flows();
};


CLICK_ENDDECLS
#endif
