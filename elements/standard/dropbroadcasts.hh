#ifndef CLICK_DROPBROADCASTS_HH
#define CLICK_DROPBROADCASTS_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * DropBroadcasts
 * =s dropping
 * drops link-level broadcast and multicast packets
 * =d
 * Drop packets that arrived as link-level broadcast or multicast.
 * Used to implement the requirement that IP routers not forward
 * link-level broadcasts.
 * Looks at the packet_type_anno annotation, which FromDevice sets.
 * =a FromDevice
 */

class DropBroadcasts : public Element {
 public:
  
  DropBroadcasts();
  ~DropBroadcasts();
  
  const char *class_name() const	{ return "DropBroadcasts"; }
  const char *processing() const	{ return "a/ah"; }
  void notify_noutputs(int);
  DropBroadcasts *clone() const;
  void add_handlers();

  uint32_t drops() const		{ return _drops; }

  void drop_it(Packet *);
  Packet *simple_action(Packet *);

private:
  uatomic32_t _drops;
};

CLICK_ENDDECLS
#endif
