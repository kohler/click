#ifndef CLICK_CHECKIP6HEADER_HH
#define CLICK_CHECKIP6HEADER_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * CheckIP6Header([BADADDRS, OFFSET])
 * =s ip6
 *
 * =d
 *
 * Expects IP6 packets as input starting at OFFSET bytes. Default OFFSET
 * is zero. Checks that the packet's length is
 * reasonable, and that the IP6 version,  length, are valid. Checks that the
 * IP6 source address is a legal unicast address. Shortens packets to the IP6
 * length, if the IP length is shorter than the nominal packet length (due to
 * Ethernet padding, for example). Pushes invalid packets out on output 1,
 * unless output 1 was unused; if so, drops invalid packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item BADADDRS
 *
 * The BADADDRS argument is a space-separated list of IP6 addresses that are
 * not to be tolerated as source addresses. 0::0 is a bad address for routers,
 * for example, but okay for link local packets.
 *
 * =item OFFSET
 *
 * Unsigned integer. Byte position at which the IP6 header begins. Default is 0.
 *
 * =back
 *
 * =a MarkIP6Header */

class CheckIP6Header : public Element {

  int _offset;

  int _n_bad_src;
  IP6Address *_bad_src; // array of illegal IP6 src addresses.
#ifdef CLICK_LINUXMODULE
  bool _aligned;
#endif
  int _drops;

 public:

  CheckIP6Header();
  ~CheckIP6Header();

  const char *class_name() const		{ return "CheckIP6Header"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PROCESSING_A_AH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  int drops() const				{ return _drops; }


  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);
  void drop_it(Packet *);


};

CLICK_ENDDECLS
#endif
