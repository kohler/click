#ifndef CHECKSRHEADER_HH
#define CHECKSRHEADER_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * CheckSRHeader()
 * =s SR
 * =d
 * Expects SR packets as input.
 * Checks that the packet's length is reasonable,
 * and that the SR header length, length, and
 * checksum fields are valid.
 *
 * =a
 * SetSRChecksum
 */

class CheckSRHeader : public Element {

  int _drops;
  
 public:
  
  CheckSRHeader();
  ~CheckSRHeader();
  
  const char *class_name() const		{ return "CheckSRHeader"; }
  const char *processing() const		{ return "a/ah"; }
  
  void notify_noutputs(int);

  int drops() const				{ return _drops; }
  
  void add_handlers();

  Packet *simple_action(Packet *);
  void drop_it(Packet *);

};

CLICK_ENDDECLS
#endif
