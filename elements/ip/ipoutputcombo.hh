#ifndef IPOUTPUTCOMBO_HH
#define IPOUTPUTCOMBO_HH

/*
 * =c
 * IPOutputCombo(COLOR, IPADDR, MTU)
 * =s output combo for IP routing
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

#include "element.hh"
#include "glue.hh"
#include "click_ip.h"

class Address;

class IPOutputCombo : public Element {
  
 public:
  
  IPOutputCombo();
  ~IPOutputCombo();
  
  const char *class_name() const		{ return "IPOutputCombo"; }
  const char *processing() const		{ return PUSH; }
  
  IPOutputCombo *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  void push(int, Packet *);
  
 private:

  int _color;			// PaintTee
  struct in_addr _my_ip;	// IPGWOptions, FixIPSrc
  unsigned _mtu;		// Fragmenter
  
};

#endif
