#ifndef CLICK_FIXDSTLOC_HH
#define CLICK_FIXDSTLOC_HH
#include <click/element.hh>
#include <click/glue.hh>
#include "loctable.hh"
CLICK_DECLS

/*
 * =c
 * FixDstLoc(LocationTable)
 * =s Grid
 * =d
 *
 * Expects a GRID_NBR_ENCAP packet with MAC header as input.  Sets the
 * packet's destination according to the destination IP address.  Takes
 * a LocationTable element as its argument.
 *
 * =a */

class FixDstLoc : public Element {

public:
  FixDstLoc();
  ~FixDstLoc();
  
  const char *class_name() const		{ return "FixDstLoc"; }
  const char *processing() const		{ return AGNOSTIC; }
  FixDstLoc *clone() const;

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

private:
  LocationTable *_loctab;
};

CLICK_ENDDECLS
#endif
