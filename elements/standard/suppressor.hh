#ifndef CLICK_SUPPRESSOR_HH
#define CLICK_SUPPRESSOR_HH
#include <click/element.hh>
#include <click/bitvector.hh>
CLICK_DECLS

/*
 * =c
 * Suppressor
 * =s classification
 * passes packets unchanged, optionally dropping some input ports
 * =d
 *
 * Suppressor has I<n> inputs and I<n> outputs. It generally passes packets
 * from input I<i> to output I<i> unchanged. However, any input port can be
 * suppressed, through a handler or a method call by another element. Packets
 * arriving on suppressed push input ports are dropped; pull requests arriving
 * on suppressed pull output ports are ignored.
 *
 * =h active0...activeI<N-1> read/write
 * Returns or sets whether each port is active (that is, not suppressed).
 * Every port starts out active.
 *
 * =h reset write-only
 * Resets every port to active. */

class Suppressor : public Element { public:

  Suppressor();

  const char *class_name() const		{ return "Suppressor"; }
  const char *port_count() const		{ return "-/="; }
  const char *flow_code() const			{ return "#/#"; }

  int configure(Vector<String> &conf, ErrorHandler *errh);
  void add_handlers();

  void push(int port, Packet *p);
  Packet *pull(int port);

  bool suppressed(int output) const { return _suppressed[output]; }
  void suppress(int output) { _suppressed[output] = true; }
  void allow(int output)    { _suppressed[output] = false; }
  void allow_all()          { _suppressed.clear(); }
  void set(int output, bool suppressed) { _suppressed[output] = suppressed; }

  private:

    Bitvector _suppressed;

};

CLICK_ENDDECLS
#endif
