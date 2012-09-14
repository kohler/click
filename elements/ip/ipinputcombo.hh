#ifndef CLICK_IPINPUTCOMBO_HH
#define CLICK_IPINPUTCOMBO_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
=c

IPInputCombo(COLOR [, BADSRC, I<keywords> INTERFACES, BADSRC, GOODDST])

=s ip

input combo for IP routing

=d

A single element encapsulating common tasks on an IP router's input path.
Effectively equivalent to

  elementclass IPInputCombo { $COLOR, $BADADDRS |
    input[0] -> Paint($COLOR)
          -> Strip(14)
          -> CheckIPHeader($BADADDRS)
          -> GetIPAddress(16)
          -> [0]output;
  }

The INTERFACES, BADSRC, and GOODDST keyword arguments correspond to
CheckIPHeader's versions.

=a Paint, CheckIPHeader, Strip, GetIPAddress, IPOutputCombo
*/

class IPInputCombo : public Element {

  atomic_uint32_t _drops;
  int _color;

  Vector<IPAddress> _bad_src;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned;
#endif
  Vector<IPAddress> _good_dst;

 public:

  IPInputCombo() CLICK_COLD;
  ~IPInputCombo() CLICK_COLD;

  const char *class_name() const		{ return "IPInputCombo"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *flags() const			{ return "A"; }

  uint32_t drops() const			{ return _drops; }
  void add_handlers() CLICK_COLD;
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

};

CLICK_ENDDECLS
#endif
