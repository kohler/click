#ifndef CLICK_COUNTERRORS_HH
#define CLICK_COUNTERRORS_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

class CountErrors : public Element { public:
  
  CountErrors();
  ~CountErrors();
  
  const char *class_name() const		{ return "CountErrors"; }
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

  int _ok_bytes;
  int _error_bytes;
  int _correct_threshold;
};

CLICK_ENDDECLS
#endif
