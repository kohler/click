/* -*- c-basic-offset: 2 -*- */
#ifndef CLICK_ICMPPINGREWRITER_HH
#define CLICK_ICMPPINGREWRITER_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/bighashmap.hh>
#include <click/ipflowid.hh>
#include <clicknet/ip.h>
CLICK_DECLS

/*
=c

ICMPPingRewriter(NEWSRC, NEWDST)

=s ICMP

rewrites ICMP echo requests and replies

=d

Rewrites ICMP echo requests and replies by changing their source and/or
destination addresses. This lets pings pass through a NAT gateway.

Expects ICMP echo requests and echo replies. Each ICMP echo request is
rewritten to have source IP address NEWSRC and destination IP address NEWDST.
However, if either address is a single dash `C<->', the corresponding field in
the IP header won't be changed. The ICMP `identifier' field is also rewritten
to a unique number. Replies to the rewritten request are themselves rewritten;
the rewritten replies look like they were responding to the original request.

ICMPPingRewriter actually keeps a table of mappings. Each mapping changes
a given (source address, destination address, identifier) triple into another
triple. Say that ICMPPingRewriter receives a request packet with triple
(I<src>, I<dst>, I<ident>), and chooses for it a new triple, (I<src2>,
I<dst2>, I<ident2>). The rewriter will then store two mappings in the table.
The first mapping changes requests (I<src>, I<dst>, I<ident>) into requests
(I<src2>, I<dst2>, I<ident2>). The second mapping changes I<replies> (I<dst2>, I<src2>, I<ident2>) into replies (I<dst>, I<src>, I<ident>). Mappings are removed if they go unused for 5 minutes.

ICMPPingRewriter may have one or two outputs. If it has two outputs,
then requests are emitted on output 0, replies on output 1. Otherwise,
all packets are emitted on output 0.

It may also have one or two inputs. They differ in how unexpected packets
are handled. On the first input, echo requests with no corresponding
mapping cause new mappings to be created, while echo replies with no
corresponding mapping are passed along unchanged. On the second input,
echo requests or replies with no corresponding mapping are simply dropped.

=a

IPRewriter, ICMPPingResponder */

class ICMPPingRewriter : public Element { public:

  ICMPPingRewriter();
  ~ICMPPingRewriter();

  const char *class_name() const	{ return "ICMPPingRewriter"; }
  const char *processing() const	{ return PUSH; }
  ICMPPingRewriter *clone() const	{ return new ICMPPingRewriter; }
  
  void notify_ninputs(int);
  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();

  void push(int, Packet *);
  void run_scheduled();

  class Mapping;
  Mapping *get_mapping(bool is_request, const IPFlowID &flow) const;
  
  class Mapping {
    
    IPFlowID _mapto;
    Mapping *_reverse;
    bool _used;
    bool _is_reverse;
    unsigned short _ip_csum_delta;
    unsigned short _icmp_csum_delta;

   public:
    
    Mapping();

    void initialize(const IPFlowID &, const IPFlowID &, bool, Mapping *);
    static void make_pair(const IPFlowID &, const IPFlowID &,
			  Mapping *, Mapping *);

    const IPFlowID &flow_id() const	{ return _mapto; }
    bool is_forward() const		{ return !_is_reverse; }
    bool is_reverse() const		{ return _is_reverse; }
    Mapping *reverse() const		{ return _reverse; }
    bool used() const			{ return _used; }
  
    void mark_used()			{ _used = true; }
    void clear_used()			{ _used = false; }
    
    void apply(WritablePacket *);

    String s() const;
    
  };
  
 protected:

  typedef BigHashMap<IPFlowID, Mapping *> Map;

  Map _request_map;
  Map _reply_map;
  Timer _timer;

  IPAddress _new_src;
  IPAddress _new_dst;
  unsigned short _identifier;

  enum { GC_INTERVAL_SEC = 300 };

  Mapping *apply_pattern(const IPFlowID &);

  static String dump_mappings_handler(Element *, void *);
  
  // void take_state_map(Map &, const Vector<Pattern *> &, const Vector<Pattern *> &);

};

CLICK_ENDDECLS
#endif
