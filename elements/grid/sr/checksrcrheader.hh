#ifndef CHECKSRCRHEADER_HH
#define CHECKSRCRHEADER_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * CheckSRCRHeader()
 * =s SRCR
 * =d
 * Expects SRCR packets as input.
 * Checks that the packet's length is reasonable,
 * and that the SRCR header length, length, and
 * checksum fields are valid.
 *
 * =a
 * SetSRCRChecksum
 */

class CheckSRCRHeader : public Element {

  int _drops;
  
 public:
  
  CheckSRCRHeader();
  ~CheckSRCRHeader();
  
  const char *class_name() const		{ return "CheckSRCRHeader"; }
  const char *processing() const		{ return "a/ah"; }
  
  CheckSRCRHeader *clone() const;
  void notify_noutputs(int);

  int drops() const				{ return _drops; }
  
  void add_handlers();

  Packet *simple_action(Packet *);
  void drop_it(Packet *);

};

CLICK_ENDDECLS
#endif
