#ifndef LINKTESTRECEIVER_HH
#define LINKTESTRECEIVER_HH

/*
 * =c
 * LinkTestReceiver([AiroInfo])
 * =s Grid
 *
 * =d
 *
 * Expects packets sent by LinkTester as input.  Print packet header
 * contents.  If the optional AiroInfo element argument is suplied,
 * queries card for and prints signal and noise information for each
 * packet.
 *
 * =a AiroInfo, LinkTester */

#include <click/element.hh>
CLICK_DECLS

class AiroInfo;

class LinkTestReceiver : public Element {

public:
  LinkTestReceiver();
  ~LinkTestReceiver();

  const char *class_name() const { return "LinkTestReceiver"; }
  const char *processing() const { return AGNOSTIC; }

  LinkTestReceiver *clone() const { return new LinkTestReceiver; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

private:
  AiroInfo *_ai;
};

CLICK_ENDDECLS
#endif
