#ifndef CLICK_BUFCONV_HH
#define CLICK_BUFCONV_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

/*
 * =c
 * BufferConverter(MTU)
 * =s TCP
 * converts TCP packets to/from buffers through handlers
 * =d
 *
 * MTU is the maximum MTU allowed. BufferConverter takes ICMP path mtu
 * discovery packets on input port 1.
 *
 * XXX - do MTU discovery etc
 */

class BufferConverter : public Element {
  static const int packet_tx_delay = 5; // 5 ms tx delay for mtu purposes
  
  static String data_read_handler(Element *e, void *);
  static int data_write_handler
    (const String &, Element *, void *, ErrorHandler *);

  Timer _timer;
  int _mtu;
  String _obuf;

  String iput();
  void oput(const String &);

public:
  BufferConverter();
  ~BufferConverter();
  
  const char *class_name() const	{ return "BufferConverter"; }
  const char *processing() const	{ return "lh/h"; }
  BufferConverter *clone() const	{ return new BufferConverter; }

  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);

  void push(int, Packet *p);
  void run_scheduled();
  void add_handlers();
};

CLICK_ENDDECLS
#endif
