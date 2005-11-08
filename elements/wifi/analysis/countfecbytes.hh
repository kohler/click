#ifndef CLICK_COUNTFECBYTES_HH
#define CLICK_COUNTFECBYTES_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

class CountFecBytes : public Element { public:
  
  CountFecBytes();
  ~CountFecBytes();
  
  const char *class_name() const		{ return "CountFecBytes"; }
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
  uint16_t _et;
  EtherAddress _src;
  int good_packet;
  bool _runs;

  int _bytes;

  int _tolerate;
  int _overhead;
  bool _adaptive;

  int _min_errors;
  int _max_errors;
  int _sum_errors;
  int _packet_count;
};

CLICK_ENDDECLS
#endif
