#ifndef CLICK_SETCRC32_HH
#define CLICK_SETCRC32_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SetCRC32()
 * =s modification
 * calculates CRC32 and prepends to packet
 * =d
 * Computes a CRC32 over each packet and appends the 4 CRC
 * bytes to the packet.
 * =a CheckCRC32
 */

class EtherAddress;

class SetCRC32 : public Element { public:
  
  SetCRC32();
  ~SetCRC32();

  const char *class_name() const	{ return "SetCRC32"; }
  const char *processing() const	{ return AGNOSTIC; }
  SetCRC32 *clone() const		{ return new SetCRC32; }
  
  Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
