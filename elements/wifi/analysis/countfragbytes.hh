#ifndef CLICK_COUNTFRAGBYTES_HH
#define CLICK_COUNTFRAGBYTES_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

class CountFragBytes : public Element { public:
  
  CountFragBytes();
  ~CountFragBytes();
  
  const char *class_name() const		{ return "CountFragBytes"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void push (int port, Packet *p_in);

  void add_handlers();

  EtherAddress _bcast;

  int _sum_signal;
  int _sum_noise;
  int _packets;

  unsigned _length;
  unsigned _overhead;
  unsigned _et;
  EtherAddress _src;
  int good_packet;
  bool _runs;

  unsigned _frag_size;
  int _bytes;

  bool _adaptive;
  int _adaptive_window;

  int _adaptive_window_bytes;
  int _adaptive_window_packets;

  int _adaptive_window_larger_bytes;

  int _adaptive_window_smaller_bytes;
  int _adaptive_window_smaller_packets;

  int _adaptive_window_smaller_sample;
};

CLICK_ENDDECLS
#endif
