#ifndef PRINTGRID_HH
#define PRINTGRID_HH
#include <click/element.hh>
#include <click/string.hh>
#include "grid.hh"
CLICK_DECLS

/*
 * =c
 * PrintGrid([LABEL] [, KEYWORDS])
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
 * =item SHOW_PROBE_CONTENTS
 *
 * Boolean. Default false.  If true, print all the entries in each link probe.
 *
 * =item VERBOSE
 *
 * Boolean.  Default true.  If false, leave out some details such as location info, etc.
 *
 * =item TIMESTAMP
 *
 * Boolean.  Default false.  If true, print the packet timestamp.
 *
 * =item PRINT_ETH
 *
 * Boolean.  Default false.  If true, print the ethernet header.
 *
 * =a
 * Print
 */

class PrintGrid : public Element {

  String _label;

 public:

  PrintGrid() CLICK_COLD;
  ~PrintGrid() CLICK_COLD;

  const char *class_name() const		{ return "PrintGrid"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

private:
  String encap_to_string(const grid_nbr_encap *) const;

  bool _print_routes;
  String get_entries(const grid_hello *) const;

  bool _print_probe_entries;
  String get_probe_entries(const grid_link_probe *) const;

  void print_ether_linkstat(Packet *) const;

  bool _verbose;
  bool _timestamp;
  bool _print_eth;
};

CLICK_ENDDECLS
#endif
