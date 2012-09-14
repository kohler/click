#ifndef CLICK_FIXIPSRC_HH
#define CLICK_FIXIPSRC_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * FixIPSrc(IPADDR)
 * =s ip
 * sets IP source field if requested by annotation
 * =d
 *
 * Expects an IP packet as input. If its Fix IP Source annotation is set, then
 * changes its IP source address field to IPADDR and recomputes the checksum.
 * Used by elements such as ICMPError that are required by standards to use
 * the IP address on the outgoing interface as the source. Such elements must
 * set ip_src to something reasonable in case the outgoing interface has no IP
 * address.
 *
 * Also clears the Fix IP Source annotation.
 *
 * =n
 *
 * The Fix IP Source annotation is stored as user annotation 3.
 *
 * =a ICMPError */

class FixIPSrc : public Element {
  struct in_addr _my_ip;

public:
  FixIPSrc() CLICK_COLD;
  ~FixIPSrc() CLICK_COLD;

  const char *class_name() const		{ return "FixIPSrc"; }
  const char *port_count() const		{ return PORTS_1_1; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  WritablePacket *fix_it(Packet *);
  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
