#ifndef DROPBROADCASTS_HH
#define DROPBROADCASTS_HH
#include <click/element.hh>

/*
 * =c
 * DropBroadcasts
 * =s
 * drops link-level broadcast packets
 * V<dropping>
 * =d
 * Drop packets that arrived as link-level broadcast or multicast.
 * Used to implement the requirement that IP routers not forward
 * link-level broadcasts.
 * Looks at the mac_broadcast annotation, which FromDevice sets.
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

  int drops() const			{ return _drops; }

  void drop_it(Packet *);
  Packet *simple_action(Packet *);

private:
  int _drops;
};

#endif
