#ifndef FIXSRCLOC_HH
#define FIXSRCLOC_HH

/*
 * =c
 * FixSrcLoc
 * =d
 *
 * Expects a Grid MAC layer packet as input.  Sets the packet's source
 * location to the Grid node's location.  This element requires a
 * LocationInfo element in the configuration.
 *
 * =a LocationInfo 
 */

#include "element.hh"
#include "glue.hh"
#include "locationinfo.hh"

class FixSrcLoc : public Element {

public:
  FixSrcLoc();
  ~FixSrcLoc();
  
  const char *class_name() const		{ return "FixSrcLoc"; }
  const char *processing() const		{ return AGNOSTIC; }
  FixSrcLoc *clone() const;

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

private:
  LocationInfo *_locinfo;
};

#endif
