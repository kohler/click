#ifndef CHECKIPHEADER_HH
#define CHECKIPHEADER_HH

/*
 * =c
 * CheckIPHeader([BADADDRS])
 * =d
 *
 * Expects IP packets as input. Checks that the packet's length is reasonable,
 * and that the IP version, header length, length, and checksum fields are
 * valid. Checks that the IP source address is a legal unicast address.
 * Shortens packets to the IP length, if the IP length is shorter than the
 * nominal packet length (due to Ethernet padding, for example). Pushes
 * invalid packets out on output 1, unless output 1 was unused; if so, drops
 * invalid packets.
 *
 * The BADADDRS argument is a space-separated list of IP addresses
 * that are not to be tolerated as source addresses.
 * Usually consists of the subnet broadcast addresses on
 * each interface (RFC1812 5.3.7).
 *
 * =a CheckIPHeader2
 * =a MarkIPHeader
 * =a SetIPChecksum
 * =a StripIPHeader */

#include "element.hh"
#include "glue.hh"

class CheckIPHeader : public Element {

  int _n_bad_src;
  u_int *_bad_src; // array of illegal IP src addresses.
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
