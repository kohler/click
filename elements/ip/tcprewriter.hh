#ifndef TCPREWRITER_HH
#define TCPREWRITER_HH
#include "elements/ip/iprw.hh"

class TCPRewriter : public IPRw {

  class TCPMapping : public Mapping {

    unsigned _seqno_delta;
    unsigned _ackno_delta;

    void change_udp_csum_delta(unsigned old_word, unsigned new_word);
    
   public:

    TCPMapping();

    void update_seqno_delta(int);
    void update_ackno_delta(int);
    
    void apply(WritablePacket *p);
    
  };
  
  Map _tcp_map;

  Vector<InputSpec> _input_specs;
  Timer _timer;

  static const int GC_INTERVAL_SEC = 3600;

  static String dump_mappings_handler(Element *, void *);
  static String dump_patterns_handler(Element *, void *);
  
 public:

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
  void add_handlers();
  void take_state(Element *, ErrorHandler *);
  
  void run_scheduled();

  TCPMapping *apply_pattern(Pattern *, int, int, bool tcp, const IPFlowID &);
  TCPMapping *get_mapping(bool, const IPFlowID &) const;
  
  void push(int, Packet *);

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
