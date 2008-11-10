#ifndef CLICK_IDLE_HH
#define CLICK_IDLE_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * Idle
 * =s basicsources
 * discards packets
 * =d
 *
 * Has zero or more agnostic outputs and zero or more agnostic inputs. It
 * never pushes a packet to any output or pulls a packet from any input. Any
 * packet it does receive is discarded. Used to avoid "input not connected"
 * error messages.
 */

class Idle : public Element { public:

  Idle();
  ~Idle();

  const char *class_name() const	{ return "Idle"; }
  const char *port_count() const	{ return "-/-"; }
  const char *processing() const	{ return "a/a"; }
  const char *flow_code() const		{ return "x/y"; }
  void *cast(const char *);
  const char *flags() const		{ return "S0"; }

  void push(int, Packet *);
  Packet *pull(int);

  private:

    Notifier _notifier;

};

CLICK_ENDDECLS
#endif
