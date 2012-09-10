#ifndef CLICK_B8B10_HH
#define CLICK_B8B10_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * B8B10(ENCODE)
 *
 * If ENCODE is 1, encode each packet with a 8b10b code.
 * If ENCODE is 0, decode.
 *
 * Encodes each 8-bit byte into a 10-bit symbol with as
 * many 0s as 1s. The point is to keep the BIM-4xx-RS232
 * radio happy.
 */

class B8B10 : public Element {
public:
  B8B10();
  ~B8B10();

  const char *class_name() const	{ return "B8B10"; }
  const char *port_count() const	{ return PORTS_1_1; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  int _flag;

};

CLICK_ENDDECLS
#endif
