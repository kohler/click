#ifndef PRINTGRID_HH
#define PRINTGRID_HH
#include <click/element.hh>
#include <click/string.hh>
#include "grid.hh"
CLICK_DECLS

/*
 * =c
 * PrintGrid([TAG] [, KEYWORDS])
 * =s Grid
 * =d
 * Assumes input packets have Ethernet headers enclosing Grid
 * packets, as described by grid_hdr in grid.hh. Prints out
 * a description of the Grid payload.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item SHOW_ROUTES
 *
 * Boolean.  Default false.  If true, print all the entries in each route advertisement.
 *
 * =item VERBOSE
 *
 * Boolean.  Default true.  If false, leave out some details such as location info, etc.
 *
 * =item TIMESTAMP
 *
 * Boolean.  Default false.  If true, print the packet timestamp.
 * 
 * =a
 * Print
 */

class PrintGrid : public Element {
  
  String _label;
  
 public:
  
  PrintGrid();
  ~PrintGrid();
  
  const char *class_name() const		{ return "PrintGrid"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  PrintGrid *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

private:
  String encap_to_string(grid_nbr_encap *);
  
  bool _print_routes;
  String get_entries(grid_hello *);

  bool _verbose;  
  bool _timestamp;
};

CLICK_ENDDECLS
#endif
