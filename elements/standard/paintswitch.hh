#ifndef CLICK_PAINTSWITCH_HH
#define CLICK_PAINTSWITCH_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

PaintSwitch

=s classification

sends packet stream to output chosen per-packet

=d

PaintSwitch sends every incoming packet to one of its output ports --
specifically, output port number K, where K is the value of the
incoming packet's Paint annotation.  If the paint annotation is set to
255, the packet is duplicated and sent to all outputs.  Since there
are only 256 different paint annotations, including the special value
of 255, PaintSwitch can have up to 255 outputs.  If there is no output
port K, the packet is dropped.

=a StaticSwitch, PullSwitch, RoundRobinSwitch, StrideSwitch, HashSwitch,
RandomSwitch, Paint, PaintTee */

class PaintSwitch : public Element { public:

  PaintSwitch();
  ~PaintSwitch();
  
  const char *class_name() const		{ return "PaintSwitch"; }
  const char *processing() const		{ return PUSH; }
  
  PaintSwitch *clone() const;
  void notify_noutputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  void configuration(Vector<String> &) const;
  bool can_live_reconfigure() const		{ return true; }
  
  void push(int, Packet *);
  
 private:

  static String read_param(Element *, void *);
  static int write_param(const String &, Element *, void *, ErrorHandler *);
  
};

CLICK_ENDDECLS
#endif
