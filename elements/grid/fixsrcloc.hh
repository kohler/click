#ifndef FIXSRCLOC_HH
#define FIXSRCLOC_HH

/*
 * =c
 * FixSrcLoc(LOCINFO)
 * =s Grid
 * =d
 *
 * Expects a Grid MAC layer packet as input.  Sets the packet's source
 * location to the Grid node's location.  This element uses the
 * GridGenericLocInfo element named LOCINFO.
 *
 * =a
 * GridLocationInfo , GridLocationInfo2
 */

#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

class GridGenericLocInfo;

class FixSrcLoc : public Element {

public:
  FixSrcLoc();
  ~FixSrcLoc();
  
  const char *class_name() const		{ return "FixSrcLoc"; }
  const char *processing() const		{ return AGNOSTIC; }
  FixSrcLoc *clone() const;

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

private:
  GridGenericLocInfo *_locinfo;
};

CLICK_ENDDECLS
#endif
