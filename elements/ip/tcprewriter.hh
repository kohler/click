#ifndef TCPREWRITER_HH
#define TCPREWRITER_HH
#include "elements/ip/iprw.hh"

/*
 * =c
 * TCPRewriter(INPUTSPEC1, ..., INPUTSPECn)
 * =s TCP
 * rewrites TCP packets' addresses, ports, and sequence numbers
 * =d
 *
 * Rewrites TCP flows by changing their source address, source port,
 * destination address, and/or destination port, and optionally, their
 * sequence numbers and acknowledgement numbers.
 *
 * This element is an IPRewriter-like element. Please read the IPRewriter
 * documentation for more information and a detailed description of its
 * INPUTSPEC arguments.
 *
 * In addition to IPRewriter's functionality, the TCPRewriter element can add
 * or subtract amounts from incoming packets' sequence and acknowledgement
 * numbers. Each newly created mapping starts with these deltas at zero; other
 * elements can request changes to a given mapping. For example, FTPPortMapper
 * uses this facility.
 *
 * =h mappings read-only
 * Returns a human-readable description of the IPRewriter's current set of
 * mappings.
 *
 * =a IPRewriter, IPRewriterPatterns, FTPPortMapper */

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
  
  void run_scheduled();

  TCPMapping *apply_pattern(Pattern *, int, int, bool tcp, const IPFlowID &);
  TCPMapping *get_mapping(bool, const IPFlowID &) const;
  
  void push(int, Packet *);

  void add_handlers();
  int llrpc(unsigned, void *);

 private:
  
  Map _tcp_map;

  Vector<InputSpec> _input_specs;
  Timer _timer;

  static const int GC_INTERVAL_SEC = 3600;

  static String dump_mappings_handler(Element *, void *);
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
TCPRewriter::get_mapping(bool tcp, const IPFlowID &in) const
{
  return (tcp ? static_cast<TCPMapping *>(_tcp_map[in]) : 0);
}

#endif
