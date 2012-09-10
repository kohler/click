#ifndef CLICK_IPOUTPUTCOMBO_HH
#define CLICK_IPOUTPUTCOMBO_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
CLICK_DECLS

/*
 * =c
 * IPOutputCombo(COLOR, IPADDR, MTU)
 * =s ip
 * output combo for IP routing
 * =d
 * A single element encapsulating common tasks on an IP router's output path.
 * Effectively equivalent to
 *
 *   elementclass IPOutputCombo { $COLOR, $IPADDR, $MTU |
 *     input[0] -> DropBroadcasts
 *           -> p::PaintTee($COLOR)
 *           -> g::IPGWOptions($IPADDR)
 *           -> FixIPSrc($IPADDR)
 *           -> d::DecIPTTL
 *           -> l::CheckLength($MTU)
 *           -> [0]output;
 *     p[1] -> [1]output;
 *     g[1] -> [2]output;
 *     d[1] -> [3]output;
 *     l[1] -> [4]output;
 *   }
 *
 * Output 0 is the path for normal packets; outputs 1 through 3 are error
 * outputs for PaintTee, IPGWOptions, and DecIPTTL, respectively; and
 * output 4 is for packets longer than MTU.
 *
 * =n
 *
 * IPOutputCombo does no fragmentation. You'll still need an IPFragmenter for
 * that.
 *
 * =a DropBroadcasts, PaintTee, CheckLength, IPGWOptions, FixIPSrc, DecIPTTL,
 * IPFragmenter, IPInputCombo */

class IPOutputCombo : public Element {

 public:

  IPOutputCombo() CLICK_COLD;
  ~IPOutputCombo() CLICK_COLD;

  const char *class_name() const		{ return "IPOutputCombo"; }
  const char *port_count() const		{ return "1/5"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  void push(int, Packet *);

 private:

  int _color;			// PaintTee
  struct in_addr _my_ip;	// IPGWOptions, FixIPSrc
  unsigned _mtu;		// Fragmenter

};

CLICK_ENDDECLS
#endif
