#ifndef CLICK_PACKETLOGGER_HH
#define CLICK_PACKETLOGGER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/dequeue.hh>

CLICK_DECLS

/*
 * =c
 * 
 * PacketLogger(ETHERTYPE ethertype)
 * 
 * =s wifi
 * 
 * expects packets with ethernet headers.  records timestamp, source
 * MAC address, and the first few bytes of every packet to pass through.
 * the list of recorded data can be dumped by repeated calls to the
 * read handler 'packets'.
 *
 * this is for logging received experiment packets on the roofnet.
 *
 * an NBYTES keyword would be nice.
 *
 * =over 8
 *
 */

class PacketLogger : public Element { public:
  
  PacketLogger();
  ~PacketLogger();
  
  const char *class_name() const		{ return "PacketLogger"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  PacketLogger *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();
  static String print_log(Element *, void *);

  enum { NBYTES = 8 };

  struct log_entry {
    struct timeval timestamp;
    uint8_t src_mac[6];
    uint8_t bytes[NBYTES];
  };
  
 private:
  
  unsigned int _et;
  unsigned int _nb;
  
  DEQueue<log_entry> _p;

};

CLICK_ENDDECLS
#endif
