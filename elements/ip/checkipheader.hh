#ifndef CHECKIPHEADER_HH
#define CHECKIPHEADER_HH

/*
 * =c
 * CheckIPHeader([BADADDRS, OFFSET])
 * =s
 * checks IP header
 * V<checking>
 * =d
 *
 * Input packets should have IP headers starting OFFSET bytes in. Default
 * OFFSET is zero. Checks that the packet's length is reasonable, and that the
 * IP version, header length, length, and checksum fields are valid. Checks
 * that the IP source address is a legal unicast address -- that is, that it
 * is not 0.0.0.0 or 255.255.255.255, or a local broadcast addresses in ADDRS.
 * Shortens packets to the IP length, if the IP length is shorter than the
 * nominal packet length (due to Ethernet padding, for example). Pushes
 * invalid packets out on output 1, unless output 1 was unused; if so, drops
 * invalid packets.
 *
 * The BADADDRS argument is a space-separated list of IP addresses that are
 * not to be tolerated as source addresses. Usually consists of the subnet
 * broadcast addresses on each interface (RFC1812 5.3.7).
 *
 * Pass a single dash `C<->' as the ADDRS argument if you don't want to check
 * the source address at all (not even against 0.0.0.0 or 255.255.255.255).
 *
 * =a CheckIPHeader2, MarkIPHeader, SetIPChecksum, StripIPHeader */

#include "element.hh"
#include "glue.hh"

class CheckIPHeader : public Element {

  int _n_bad_src;
  u_int *_bad_src; // array of illegal IP src addresses.
  unsigned _offset;
#ifdef __KERNEL__
  bool _aligned;
#endif
  int _drops;
  
 public:
  
  CheckIPHeader();
  ~CheckIPHeader();
  
  const char *class_name() const		{ return "CheckIPHeader"; }
  const char *processing() const		{ return "a/ah"; }
  
  CheckIPHeader *clone() const;
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  int drops() const				{ return _drops; }
  
  void add_handlers();

  Packet *simple_action(Packet *);
  void drop_it(Packet *);

};

#endif
