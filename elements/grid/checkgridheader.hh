#ifndef CHECKGRIDHEADER_HH
#define CHECKGRIDHEADER_HH

/*
 * =c
 * CheckGridHeader([BADADDRS])
 * =d
 * Expects Grid packets as input.
 * Checks that the packet's length is reasonable,
 * and that the Grid header length, length, and
 * checksum fields are valid.
 *
 * =a SetGridChecksum
 */

#include "element.hh"
#include "glue.hh"

class CheckGridHeader : public Element {

  int _drops;
  
 public:
  
  CheckGridHeader();
  ~CheckGridHeader();
  
  const char *class_name() const		{ return "CheckGridHeader"; }
  const char *processing() const		{ return "a/ah"; }
  
  CheckGridHeader *clone() const;
  void notify_noutputs(int);
  
  int drops() const				{ return _drops; }
  
  void add_handlers();

  Packet *simple_action(Packet *);
  void drop_it(Packet *);

};

#endif
