#ifndef TCPREWRITER_HH
#define TCPREWRITER_HH
#include "elements/ip/iprw.hh"

/*
=c

TCPRewriter(INPUTSPEC1, ..., INPUTSPECn [, KEYWORDS])

=s TCP

rewrites TCP packets' addresses, ports, and sequence numbers

=d

Rewrites TCP flows by changing their source address, source port,
destination address, and/or destination port, and optionally, their
sequence numbers and acknowledgement numbers.

This element is an IPRewriter-like element. Please read the IPRewriter
documentation for more information and a detailed description of its
INPUTSPEC arguments.

In addition to IPRewriter's functionality, the TCPRewriter element can add
or subtract amounts from incoming packets' sequence and acknowledgement
numbers. Each newly created mapping starts with these deltas at zero; other
elements can request changes to a given mapping. For example, FTPPortMapper
uses this facility.

Keyword arguments determine how often stale mappings should be removed.

=over 5

=item TCP_TIMEOUT I<time>

Time out TCP connections every I<time> seconds. Default is 24 hours.

=item TCP_DONE_TIMEOUT I<time>

Time out completed TCP connections every I<time> seconds. Default is 30
seconds. FIN and RST flags mark TCP connections as complete.

=item REAP_TCP I<time>

Reap timed-out TCP connections every I<time> seconds. If no packets
corresponding to a given mapping have been seen for TCP_TIMEOUT, remove the
mapping as stale. Default is 1 hour.

=item REAP_TCP_DONE I<time>

Reap timed-out completed TCP connections every I<time> seconds. Default is 10
seconds.

=back

=h mappings read-only

Returns a human-readable description of the TCPRewriter's current set of
mappings.

=a IPRewriter, IPRewriterPatterns, FTPPortMapper */

class TCPRewriter : public IPRw { public:

  class TCPMapping : public Mapping {

    unsigned _seqno_delta;
    unsigned _ackno_delta;
    unsigned _interesting_seqno;

    void change_udp_csum_delta(unsigned old_word, unsigned new_word);
    
   public:

    TCPMapping();

    TCPMapping *reverse() const		{ return static_cast<TCPMapping *>(_reverse); }

    unsigned interesting_seqno() const	{ return _interesting_seqno; }
    void set_interesting_seqno(unsigned s) { _interesting_seqno = s; }
    
    void update_seqno_delta(int);
    void update_ackno_delta(int);
    
    void apply(WritablePacket *p);

    String s() const;
    
  };

  TCPRewriter();
  ~TCPRewriter();
  
  const char *class_name() const		{ return "TCPRewriter"; }
  void *cast(const char *);
  TCPRewriter *clone() const			{ return new TCPRewriter; }
  void notify_noutputs(int);
  const char *processing() const		{ return PUSH; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void take_state(Element *, ErrorHandler *);
  
  int notify_pattern(Pattern *, ErrorHandler *);
  TCPMapping *apply_pattern(Pattern *, int ip_p, const IPFlowID &, int, int);
  TCPMapping *get_mapping(int ip_p, const IPFlowID &) const;
  
  void push(int, Packet *);

  void add_handlers();
  int llrpc(unsigned, void *);

 private:
  
  Map _tcp_map;
  Mapping *_tcp_done;
  Mapping *_tcp_done_tail;

  Vector<InputSpec> _input_specs;

  int _tcp_gc_interval;
  int _tcp_done_gc_interval;
  Timer _tcp_gc_timer;
  Timer _tcp_done_gc_timer;
  int _tcp_timeout_jiffies;
  int _tcp_done_timeout_jiffies;

  int _nmapping_failures;
  
  static void tcp_gc_hook(Timer *, void *);
  static void tcp_done_gc_hook(Timer *, void *);

  static String dump_mappings_handler(Element *, void *);
  static String dump_nmappings_handler(Element *, void *);
  static String dump_patterns_handler(Element *, void *);

};

inline void
TCPRewriter::TCPMapping::update_seqno_delta(int d)
{
  change_udp_csum_delta(htonl(_seqno_delta), htonl(_seqno_delta + d));
  _seqno_delta += d;
}

inline void
TCPRewriter::TCPMapping::update_ackno_delta(int d)
{
  change_udp_csum_delta(htonl(_ackno_delta), htonl(_ackno_delta + d));
  _ackno_delta += d;
}

inline TCPRewriter::TCPMapping *
TCPRewriter::get_mapping(int ip_p, const IPFlowID &in) const
{
  if (ip_p == IP_PROTO_TCP)
    return static_cast<TCPMapping *>(_tcp_map[in]);
  else
    return 0;
}

#endif
