#ifndef CLICK_FORCETCP_HH
#define CLICK_FORCETCP_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * ForceTCP([DPORT, RANDOM_DPORT, FLAGS])
 * =s tcp
 * sets TCP packet fields
 * =d
 * Set the checksum and some other fields to try to make a packet look like
 * TCP. If DPORT is specified and not 0, forces the destination port to be
 * DPORT. Otherwise, if RANDOM_DPORT is set, destination port may be randomly set.
 * If RANDOM_DPORT is not set and DPORT is 0, destination port is untouched. RANDOM_DPORT
 * has no effect if DPORT is not 0. If FLAGS is specified and not -1, set the
 * TCP flags field to FLAGS.
 */

class ForceTCP : public Element {
public:
  ForceTCP() CLICK_COLD;
  ~ForceTCP() CLICK_COLD;

  const char *class_name() const		{ return "ForceTCP"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  int _count;
  uint16_t _dport;
  int _flags;
  bool _random;
};

CLICK_ENDDECLS
#endif
