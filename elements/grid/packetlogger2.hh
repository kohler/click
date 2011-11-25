#ifndef CLICK_PACKETLOGGER2_HH
#define CLICK_PACKETLOGGER2_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/deque.hh>

CLICK_DECLS

/*
 * =c
 * PacketLogger2(I<KEYWORKDS>)
 *
 * =s Grid
 * Log packets for later dumping/analysis.
 *
 * =d
 * PacketLogger2 records the size, timestamp, user annotations, and
 * the first few bytes of every packet to pass through.  The list of
 * recorded data can be dumped (and cleared) by repeated calls to the
 * read handler 'log'.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item NBYTES
 * Unsigned integer.  Number of bytes of each packet to record.
 * Defaults to 34 bytes.
 *
 * =back
 *
 * =h log read-only
 * Print as much of the list of logged packets as possible, clearing
 * printed packets from the log.
 *
 */

class PacketLogger2 : public Element { public:

  PacketLogger2();
  ~PacketLogger2();

  const char *class_name() const		{ return "PacketLogger2"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flow_code() const			{ return "#/#"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String print_log(Element *, void *);
  static int clear(const String &foo, Element *e, void *vparam, ErrorHandler *errh);
  static String left(Element *, void *);

  enum { NBYTES = 18 };

  struct log_entry {
    Timestamp timestamp;
    unsigned length;
    uint8_t anno[Packet::anno_size];
    uint8_t bytes[NBYTES];
  };

 private:

  unsigned int _nb;

  Deque<log_entry> _p;

};

CLICK_ENDDECLS
#endif
