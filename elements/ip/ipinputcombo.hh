#ifndef CLICK_IPINPUTCOMBO_HH
#define CLICK_IPINPUTCOMBO_HH
#include <click/element.hh>
#include <click/ipaddresslist.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
=c

IPInputCombo(COLOR [, BADSRC, I<keywords> INTERFACES, BADSRC, GOODDST])

=s IP

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
  
  uatomic32_t _drops;
  int _color;
  
  IPAddressList _bad_src;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned;
#endif
  IPAddressList _good_dst;

 public:
  
  IPInputCombo();
  ~IPInputCombo();
  
  const char *class_name() const		{ return "IPInputCombo"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  uint32_t drops() const			{ return _drops; }
  IPInputCombo *clone() const;
  void add_handlers();
  int configure(Vector<String> &, ErrorHandler *);

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);
  
};

CLICK_ENDDECLS
#endif
