#ifndef IPOUTPUTCRUD_HH
#define IPOUTPUTCRUD_HH

/*
 * =c
 * IPOutputCrud(paint color, interface IP address, MTU)
 * =d
 * Effectively a compound element, equivalent to
 * = elementclass IPOutputCrud(color, ip, mtu) {
 * =   in[0] -> DropBroadcasts
 * =         -> c::CheckPaint(color)
 * =         -> g::IPGWOptions(ip)
 * =         -> FixIPSrc(ip)
 * =         -> d::DecIPTTL
 * =         -> f::SendToOutput1IfLongerThan(mtu)
 * =         -> [0]out;
 * =   c[1] -> [1]out;
 * =   g[1] -> [2]out;
 * =   d[1] -> [3]out;
 * =   f[1] -> [4]out;
 * = }
 * </pre>
 * =a DropBroadcasts
 * =a CheckPaint
 * =a IPGWOptions
 * =a FixIPSrc
 * =a DecIPTTL
 * =a Fragmenter
 */

#include "element.hh"
#include "glue.hh"
#include "click_ip.h"

class Address;

class IPOutputCrud : public Element {
  
 public:
  
  IPOutputCrud();
  ~IPOutputCrud();
  
  const char *class_name() const		{ return "IPOutputCrud"; }
  Processing default_processing() const	{ return PUSH; }
  
  IPOutputCrud *clone() const;
  int configure(const String &, ErrorHandler *);

  void push(int, Packet *);
  
 private:

  int _color;			// CheckPaint
  struct in_addr _my_ip;	// IPGWOptions, FixIPSrc
  unsigned _mtu;		// Fragmenter
  
};

#endif
