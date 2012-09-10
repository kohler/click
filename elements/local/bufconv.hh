#ifndef CLICK_BUFCONV_HH
#define CLICK_BUFCONV_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

/*
 * =c
 * BufferConverter(MTU)
 * =s tcp
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

  static String data_read_handler(Element *e, void *) CLICK_COLD;
  static int data_write_handler
    (const String &, Element *, void *, ErrorHandler *);

  Timer _timer;
  int _mtu;
  String _obuf;

  String iput();
  void oput(const String &);

public:
  BufferConverter() CLICK_COLD;
  ~BufferConverter() CLICK_COLD;

  const char *class_name() const	{ return "BufferConverter"; }
  const char *port_count() const	{ return "2/1"; }
  const char *processing() const	{ return "lh/h"; }

  int initialize(ErrorHandler *) CLICK_COLD;
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  void push(int, Packet *p);
  void run_timer(Timer *);
  void add_handlers() CLICK_COLD;
};

CLICK_ENDDECLS
#endif
