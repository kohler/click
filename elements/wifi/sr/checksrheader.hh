#ifndef CHECKSRHEADER_HH
#define CHECKSRHEADER_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

CheckSRHeader()

=s Wifi, Wireless Routing

Check the Source Route header of a packet.

=d

Expects SR packets as input.
Checks that the packet's length is reasonable,
and that the SR header length, length, and
checksum fields are valid.

=a SetSRChecksum
 */

class CheckSRHeader : public Element {

  typedef HashMap<EtherAddress, uint8_t> BadTable;
  typedef BadTable::const_iterator BTIter;
  
  class BadTable _bad_table;
  int _drops;

 public:
  
  CheckSRHeader();
  ~CheckSRHeader();
  
  const char *class_name() const		{ return "CheckSRHeader"; }
  const char *processing() const		{ return "a/ah"; }
  
  void notify_noutputs(int);

  int drops() const				{ return _drops; }


  Packet *simple_action(Packet *);
  void drop_it(Packet *);

  String bad_nodes();
  void add_handlers();

};

CLICK_ENDDECLS
#endif
