#ifndef FIXIPSRC_HH
#define FIXIPSRC_HH

/*
 * =c
 * FixIPSrc(ip-addr)
 * =d
 * If the fix_ip_src annotation is set, then change ip_src to
 * ip-addr and recompute the checksum. Used by elements such
 * as ICMPError that are required by standards to use the
 * IP address on the outgoing interface as the source. Such
 * elements must set ip_src to something reasonable in case
 * the outgoing interface has no IP address.
 *
 * Expects an IP packet as input. Clears the fix_ip_src annotation.
 * 
 * =a ICMPError
 */

#include "element.hh"
#include "glue.hh"

class FixIPSrc : public Element {
  struct in_addr _my_ip;

public:
  FixIPSrc();
  ~FixIPSrc();
  
  const char *class_name() const		{ return "FixIPSrc"; }
  const char *processing() const	{ return AGNOSTIC; }
  FixIPSrc *clone() const;
  int configure(const String &, ErrorHandler *);

  void fix_it(Packet *);
  Packet *simple_action(Packet *);
  
};

#endif
