#ifndef CLICK_SUPPRESSOR_HH
#define CLICK_SUPPRESSOR_HH
#include <click/element.hh>

/*
 * =c
 * Suppressor
 * =s dropping
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

class Suppressor : public Element {
  
  fd_set _suppressed;
  
 public:
  
  Suppressor();
  ~Suppressor();
  
  const char *class_name() const		{ return "Suppressor"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flow_code() const			{ return "#/#"; }
  void notify_ninputs(int);
  
  Suppressor *clone() const;
  int initialize(ErrorHandler *);
  void add_handlers();
  
  void push(int port, Packet *p);
  Packet *pull(int port);
  
  bool suppressed(int output) const { return FD_ISSET(output, &_suppressed); }
  void suppress(int output) { FD_SET(output, &_suppressed); }
  void allow(int output)    { FD_CLR(output, &_suppressed); }
  void allow_all()          { FD_ZERO(&_suppressed); }

  bool set(int output, bool suppressed);

};

#endif
