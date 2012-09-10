#ifndef CLICK_PACKETLOGGER_HH
#define CLICK_PACKETLOGGER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/deque.hh>

CLICK_DECLS

/*
 * =c
 *
 * PacketLogger(ETHERTYPE ethertype)
 *
 * =s Grid
 *
 * Log packets for later dumping/analysis.
 *
 * =d
 * expects packets with ethernet headers.  records timestamp, source
 * MAC address, and the first few bytes of every packet to pass through.
 * the list of recorded data can be dumped by repeated calls to the
 * read handler 'packets'.
 *
 * this is for logging received experiment packets on the roofnet.
 *
 * an NBYTES keyword would be nice.
 *
 */

class PacketLogger : public Element { public:

  PacketLogger() CLICK_COLD;
  ~PacketLogger() CLICK_COLD;

  const char *class_name() const		{ return "PacketLogger"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers() CLICK_COLD;
  static String print_log(Element *, void *);

  enum { NBYTES = 8 };

  struct log_entry {
    Timestamp timestamp;
    uint8_t src_mac[6];
    uint8_t bytes[NBYTES];
  };

 private:

  uint16_t _et;
  unsigned int _nb;

  Deque<log_entry> _p;

};

CLICK_ENDDECLS
#endif
