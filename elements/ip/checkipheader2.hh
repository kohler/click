#ifndef CHECKIPHEADER2_HH
#define CHECKIPHEADER2_HH

/*
 * =c
 * CheckIPHeader2([BADADDRS, OFFSET])
 * =s
 * checks IP header, no checksum
 * V<checking>
 * =d
 *
 * Input packets should have IP headers starting OFFSET bytes in. Default
 * OFFSET is zero. Checks that the packet's length is reasonable, and that the
 * IP version, header length, and length fields are valid. Checks that the IP
 * source address is a legal unicast address -- that is, that it is not
 * 0.0.0.0 or 255.255.255.255, or one of the local broadcast addresses in
 * ADDRS. Shortens packets to the IP length, if the IP length is shorter than
 * the nominal packet length (due to Ethernet padding, for example). Pushes
 * invalid packets out on output 1, unless output 1 was unused; if so, drops
 * invalid packets.
 *
 * The BADADDRS argument is a space-separated list of IP addresses
 * that are not to be tolerated as source addresses.
 * Usually consists of the subnet broadcast addresses on
 * each interface (RFC1812 5.3.7).
 *
 * Pass a single dash `C<->' as the ADDRS argument if you don't want to check
 * the source address at all (not even against 0.0.0.0 or 255.255.255.255).
 *
 * This element does all the work of CheckIPHeader except for checking
 * the checksum.
 *
 * =a CheckIPHeader, StripIPHeader, MarkIPHeader */

#include "element.hh"
#include "glue.hh"

class CheckIPHeader2 : public Element {

  int _n_bad_src;
  u_int *_bad_src; // array of illegal IP src addresses.
  unsigned _offset;
  int _drops;
  
 public:
  
  CheckIPHeader2();
  ~CheckIPHeader2();
  
  const char *class_name() const		{ return "CheckIPHeader2"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  int drops() const				{ return _drops; }
  
  CheckIPHeader2 *clone() const;
  void add_handlers();

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

};

#endif
