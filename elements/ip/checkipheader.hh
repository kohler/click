#ifndef CHECKIPHEADER_HH
#define CHECKIPHEADER_HH

/*
 * =c
 * CheckIPHeader(illegal-src-addresses)
 * =d
 * Expects IP packets as input.
 * Checks that the packet's length is reasonable,
 * and that the IP version, header length, length, and
 * checksum fields are valid.
 * Checks that the IP source address is a legal unicast address.
 * Pushes invalid packets out on output 1, unless output 1 was unused;
 * if so, drops invalid packets.
 *
 * The argument is a comma-separated list of IP addresses
 * that are not to be tolerated as source addresses.
 * Usually consists of the subnet broadcast addresses on
 * each interface. RFC1812 5.3.7.
 *
 * =a SetIPChecksum
 * =a DecIPTTL
 */

#include "element.hh"
#include "glue.hh"

class CheckIPHeader : public Element {

  int _drops;
  int _n_bad_src;
  u_int *_bad_src; // array of illegal IP src addresses.
  
 public:
  
  CheckIPHeader();
  ~CheckIPHeader();
  
  const char *class_name() const		{ return "CheckIPHeader"; }
  void notify_outputs(int);
  void processing_vector(Vector<int> &, int, Vector<int> &, int) const;
  int configure(const String &, Router *, ErrorHandler *);
  
  int drops() const				{ return _drops; }
  
  CheckIPHeader *clone() const;
  void add_handlers(HandlerRegistry *fcr);

  inline Packet *smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

};

#endif
