#ifndef CHECKIP6HEADER_HH
#define CHECKIP6HEADER_HH

/*
 * =c
 * CheckIP6Header([BADADDRS])
 * =s IPv6, checking
 * 
 * =d
 *
 * Expects IP6 packets as input. Checks that the packet's length is
 * reasonable, and that the IP6 version,  length, are valid. Checks that the
 * IP6 source address is a legal unicast address. Shortens packets to the IP6
 * length, if the IP length is shorter than the nominal packet length (due to
 * Ethernet padding, for example). Pushes invalid packets out on output 1,
 * unless output 1 was unused; if so, drops invalid packets.
 *
 * The BADADDRS argument is a space-separated list of IP6 addresses that are
 * not to be tolerated as source addresses. 0::0 is a bad address for routers,
 * for example, but okay for link local packets.
 *
 * =a MarkIP6Header */


#include <click/element.hh>
#include <click/glue.hh>

class CheckIP6Header : public Element {

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
  const char *processing() const		{ return "a/ah"; }

  CheckIP6Header *clone() const;
  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  
  int drops() const				{ return _drops; }
  
 
  void add_handlers();
  
  Packet *simple_action(Packet *);
  void drop_it(Packet *);


};

#endif
