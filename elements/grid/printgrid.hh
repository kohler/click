#ifndef PRINTGRID_HH
#define PRINTGRID_HH
#include "element.hh"
#include "string.hh"

/*
 * =c
 * PrintGrid([TAG])
 * =d
 * Assumes input packets have Ethernet headers enclosing Grid
 * packets, as described by grid_hdr in grid.hh. Prints out
 * a description of the Grid payload.
 */

class PrintGrid : public Element {
  
  String _label;
  
 public:
  
  PrintGrid();
  ~PrintGrid();
  
  const char *class_name() const		{ return "PrintGrid"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  PrintGrid *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);
  
};

#endif
