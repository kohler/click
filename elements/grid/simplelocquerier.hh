#ifndef SIMPLELOCQUERIER_HH
#define SIMPLELOCQUERIER_HH

/*
 * =c
 * SimpleLocQuerier(DEST-IP LAT LON [, ...])
 * =s Grid
 * Sets Grid destination location by looking in a static table
 * =d
 *
 * Each argument is a 3-tuple of the destination IP, latitude, and
 * longitude (lat, lon as real numbers).
 *
 * Expects GRID_NBR_ENCAP packet with MAC headers as input, which will
 * be output with the destination location filled in to match the
 * header's destination address.
 *
 * =a FloodingLocQuerier
 */

#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/bighashmap.hh>
#include "grid.hh"
CLICK_DECLS

class SimpleLocQuerier : public Element {
 public:

  SimpleLocQuerier() CLICK_COLD;
  ~SimpleLocQuerier() CLICK_COLD;

  const char *class_name() const		{ return "SimpleLocQuerier"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }
  void add_handlers() CLICK_COLD;

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void push(int port, Packet *);


 private:
  typedef HashMap<IPAddress, grid_location> locmap;
  locmap _locs;

  void send_query_for(const IPAddress &);

  static String read_table(Element *, void *);
  static int add_entry(const String &arg, Element *element, void *, ErrorHandler *errh);
};

CLICK_ENDDECLS
#endif
