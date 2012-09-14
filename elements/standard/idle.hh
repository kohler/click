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
 * Idle never pushes a packet to any output or pulls a packet from any input.
 * Any packet it does receive by push is discarded.  Idle is often used to
 * avoid "input not connected" error messages.
 *
 * Idle provides an upstream-empty notifier to inform downstream pullers that
 * it is dormant.  As a result, in the configuration
 *
 *   Idle -> SimpleQueue -> Discard;
 *
 * the Discard element will itself go dormant.  Use SimpleIdle to avoid this
 * effect.
 *
 * =sa SimpleIdle
 */

class Idle : public Element { public:

  Idle() CLICK_COLD;

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
