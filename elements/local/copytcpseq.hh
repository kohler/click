#ifndef CLICK_COPYTCPSEQ_HH
#define CLICK_COPYTCPSEQ_HH
#include <click/element.hh>
#include <clicknet/tcp.h>
#include <click/ipflowid.hh>
CLICK_DECLS

/*
 * =c
 * CopyTCPSeq()
 * =s tcp
 * sets TCP sequence number
 * =d
 *
 * remembers highest sequence number from packets between input/output 0 and
 * tag that number onto packets between input/output 1.
 */

class CopyTCPSeq : public Element {
private:
  static int reset_write_handler
    (const String &, Element *, void *, ErrorHandler *);

  void monitor(Packet *);
  Packet *set(Packet *);

  bool _start;
  unsigned _seq;

public:
  CopyTCPSeq() CLICK_COLD;
  ~CopyTCPSeq() CLICK_COLD;

  const char *class_name() const	{ return "CopyTCPSeq"; }
  const char *port_count() const	{ return "2/2"; }

  int initialize(ErrorHandler *) CLICK_COLD;
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  void push(int, Packet *);
  Packet *pull(int);
  void add_handlers() CLICK_COLD;

};

CLICK_ENDDECLS
#endif
