#ifndef FILTERBYRANGE_HH
#define FILTERBYRANGE_HH

/*
 * =c
 * FilterByRange(RANGE)
 * =d
 *
 * Expects Grid MAC layer packets on input 0.  Any packet transmitted
 * by a Grid node within RANGE metres from this node is sent to output
 * 0.  Packets transmitted by ``out of range'' nodes are sent out
 * output 1.  RANGE is an Integer.  This element is useful for
 * simulating physical topologies.  This element requires a
 * LocationInfo element in the configuration.
 *
 * =a LocationInfo */

#include "element.hh"
#include "locationinfo.hh"

class FilterByRange : public Element {
  
public:
  
  FilterByRange();
  ~FilterByRange();
  
  const char *class_name() const		{ return "FilterByRange"; }
  const char *processing() const		{ return PUSH; }
  FilterByRange *clone() const                  { return new FilterByRange; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void push(int port, Packet *);

private:
  LocationInfo *_locinfo;
  int _range; // in metres
  double calc_range(grid_location l1, grid_location l2);
};

#endif
