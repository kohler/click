#ifndef CLICK_FILTERBYRANGE_HH
#define CLICK_FILTERBYRANGE_HH
#include <click/element.hh>
#include "elements/grid/gridlocationinfo.hh"
CLICK_DECLS

/*
 * =c
 * FilterByRange(RANGE, LOCINFO)
 * =s Grid
 * =d
 *
 * Expects Grid MAC layer packets on input 0.  Any packet transmitted
 * by a Grid node within RANGE metres from this node is sent to output
 * 0.  Packets transmitted by ``out of range'' nodes are sent out
 * output 1.  RANGE is an Integer.  A negative range means no packets
 * are filtered out.  This element is useful for simulating physical
 * topologies.  This element uses the GridLocationInfo element named
 * LOCINFO.
 *
 * =a
 * GridLocationInfo
 * FilterByGridHops */

class FilterByRange : public Element {

public:

  FilterByRange() CLICK_COLD;
  ~FilterByRange() CLICK_COLD;

  const char *class_name() const		{ return "FilterByRange"; }
  const char *port_count() const		{ return "1/2"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void push(int port, Packet *);

private:
  GridLocationInfo *_locinfo;
  int _range; // in metres, negative meand don't filter
};

CLICK_ENDDECLS
#endif
