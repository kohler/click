#ifndef FILTERBYHOPS_HH
#define FILTERBYHOPS_HH

/*
 * =c
 * FilterByGridHops(NUM-HOPS)
 * =d
 *
 * Expects GRID_NBR_ENCAP packets with MAC headers on input 0.  Any
 * packet that has travelled less than NUM-HOPS so fare is sent to
 * output 0.  Packets that have already travelled NUM-HOPS or more are
 * sent out output 1.  NUM-HOPS is an Integer.  
 *
 * =a FilterByRange
 */

#include <click/element.hh>

class FilterByGridHops : public Element {
  
public:
  
  FilterByGridHops();
  ~FilterByGridHops();
  
  const char *class_name() const		{ return "FilterByGridHops"; }
  const char *processing() const		{ return PUSH; }
  FilterByGridHops *clone() const                  { return new FilterByGridHops; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void push(int port, Packet *);
private:
  unsigned int _max_hops;
};

#endif
