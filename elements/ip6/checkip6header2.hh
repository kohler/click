#ifndef CHECKIP6HEADER2_HH
#define CHECKIP6HEADER2_HH

/*
 * =c
 * CheckIP6Header2(illegal-src-addresses)
 * =d
 * Expects IP6 packets as input.
 * Checks that the packet's length is reasonable,
 * and that the IP version, header length, length, and
 * checksum fields are valid.
 * Checks that the IP source address is a legal unicast address.
 * Pushes invalid packets out on output 1, unless output 1 was unused;
 * if so, drops invalid packets.
 *
 * The argument is a space-separated list of IP addresses
 * that are not to be tolerated as source addresses.
 * Usually consists of the subnet broadcast addresses on
 * each interface (RFC1812 5.3.7).
 *
  * =a CheckIP6Header
 * =a StripIP6Header
 * =a MarkIP6Header */


#include "element.hh"
#include "glue.hh"

class CheckIP6Header2 : public Element {

  int _drops;
  int _n_bad_src;
  IP6Address *_bad_src;
 
  
 public:
  
  CheckIP6Header2();
  ~CheckIP6Header2();
  
  const char *class_name() const		{ return "CheckIP6Header2"; }
  const char *processing() const		{ return "a/ah"; }
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  int drops() const				{ return _drops; }
  
  CheckIP6Header2 *clone() const;
  void add_handlers();

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

};

#endif
