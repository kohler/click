#ifndef CHECKGRIDHEADER_HH
#define CHECKGRIDHEADER_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * CheckGridHeader([BADADDRS])
 * =s Grid
 * =d
 * Expects Grid packets as input.
 * Checks that the packet's length is reasonable,
 * and that the Grid header length, length, and
 * checksum fields are valid.
 *
 * =a
 * SetGridChecksum
 */

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

CLICK_ENDDECLS
#endif
