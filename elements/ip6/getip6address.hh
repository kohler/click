#ifndef CLICK_GETIP6ADDRESS_HH
#define CLICK_GETIP6ADDRESS_HH
#include <click/element.hh>
#include <click/ip6address.hh>
CLICK_DECLS

/*
 * =c
 * GetIP6Address(OFFSET)
 * =s ip6
 *
 * =d
 * Copies 16 bytes from the packet , starting at OFFSET, to the destination IP6
 * address annotation.  The offset is usually 24, to fetch the dst address from
 * an IP6 packet (w/o ether header).
 *
 * The destination address annotation is used by elements
 * that need to know where the packet is going.
 * Such elements include NDSol and LookupIP6Route.
 *
 * =a NDSol, LookupIP6Route, SetIP6Address, StoreIP6Address
 */


class GetIP6Address : public Element {

  int _offset;

 public:

  GetIP6Address();
  ~GetIP6Address();

  const char *class_name() const	{ return "GetIP6Address"; }
  const char *port_count() const	{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
