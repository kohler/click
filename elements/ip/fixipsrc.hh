#ifndef FIXIPSRC_HH
#define FIXIPSRC_HH

/*
 * =c
 * FixIPSrc(IPADDR)
 * =s IP
 * sets IP source field if requested by annotation
 * =d
 *
 * Expects an IP packet as input. If its fix_ip_src annotation is set, then
 * changes its IP source address field to IPADDR and recomputes the checksum.
 * Used by elements such as ICMPError that are required by standards to use
 * the IP address on the outgoing interface as the source. Such elements must
 * set ip_src to something reasonable in case the outgoing interface has no IP
 * address.
 *
 * Also clears the fix_ip_src annotation.
 * 
 * =a ICMPError */

#include <click/element.hh>
#include <click/glue.hh>

class FixIPSrc : public Element {
  struct in_addr _my_ip;

public:
  FixIPSrc();
  ~FixIPSrc();
  
  const char *class_name() const		{ return "FixIPSrc"; }
  const char *processing() const		{ return AGNOSTIC; }
  FixIPSrc *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);

  WritablePacket *fix_it(Packet *);
  Packet *simple_action(Packet *);
  
};

#endif
