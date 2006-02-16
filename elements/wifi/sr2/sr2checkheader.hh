#ifndef SR2CHECKHEADER_HH
#define SR2CHECKHEADER_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c

SR2CheckHeader()

=s Wifi, Wireless Routing

Check the Source Route header of a packet.

=d

Expects SR packets as input.
Checks that the packet's length is reasonable,
and that the SR header length, length, and
checksum fields are valid.

=a SetSRChecksum
 */

class SR2CheckHeader : public Element {

  typedef HashMap<EtherAddress, uint8_t> BadTable;
  typedef BadTable::const_iterator BTIter;
  
  BadTable _bad_table;
  int _drops;

 public:
  
  SR2CheckHeader();
  ~SR2CheckHeader();
  
  const char *class_name() const		{ return "SR2CheckHeader"; }
  const char *port_count() const		{ return "1/1-2"; }
  const char *processing() const		{ return "a/ah"; }

  int drops() const				{ return _drops; }


  Packet *simple_action(Packet *);
  void drop_it(Packet *);

  String bad_nodes();
  void add_handlers();

};

CLICK_ENDDECLS
#endif
