#ifndef FIXDSTLOC_HH
#define FIXDSTLOC_HH

/*
 * =c
 * FixDstLoc(LocationTable)
 * =s Grid
 * =d
 *
 * Expects a GRID_NBR_ENCAP packet with MAC header as input.  Sets the
 * packet's destination according to the destination IP address.  Take
 * a LocationTable element as its argument.
 *
 * =a */

#include <click/element.hh>
#include <click/glue.hh>
#include "loctable.hh"

class FixDstLoc : public Element {

public:
  FixDstLoc();
  ~FixDstLoc();
  
  const char *class_name() const		{ return "FixDstLoc"; }
  const char *processing() const		{ return AGNOSTIC; }
  FixDstLoc *clone() const;

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

private:
  LocationTale *_loctab;
};

#endif
