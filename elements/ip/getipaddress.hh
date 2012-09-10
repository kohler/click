#ifndef CLICK_GETIPADDRESS_HH
#define CLICK_GETIPADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
 * =c
 * GetIPAddress(OFFSET [, ANNO, keyword IP])
 * =s ip
 * sets destination IP address annotation from packet data
 * =d
 *
 * Copies 4 bytes from the packet, starting at OFFSET, to the destination IP
 * address annotation. OFFSET is usually 16, to fetch the destination address
 * from an IP packet.
 *
 * Set ANNO to set a non-default destination IP address annotation.
 *
 * You may also give an IP keyword argument instead of an OFFSET.  IP must
 * equal 'src' or 'dst'.  The input packet must have its IP header annotation
 * set.  The named destination IP address annotation is set to that IP
 * header's source or destination address, respectively.
 *
 * =n
 * The destination address annotation is used by elements
 * that need to know where the packet is going.
 * Such elements include ARPQuerier and LookupIPRoute.
 *
 * =a ARPQuerier, StaticIPLookup, SetIPAddress, StoreIPAddress
 */


class GetIPAddress : public Element {

    enum {
	offset_ip_src = -1,
	offset_ip_dst = -2
    };

    int _offset;
    int _anno;

 public:

  GetIPAddress() CLICK_COLD;
  ~GetIPAddress() CLICK_COLD;

  const char *class_name() const		{ return "GetIPAddress"; }
  const char *port_count() const		{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
