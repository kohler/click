#ifndef CHECKCRC32_HH
#define CHECKCRC32_HH

/*
 * =c
 * CheckCRC32()
 * =s
 * checks packet CRC32s
 * V<checking>
 * =d
 * Check that the CRC32 appended by SetCRC32 is OK.
 * If so, delete the CRC from the packet.
 * Otherwise, drop the packet.
 * =a SetCRC32
 */

#include "element.hh"

class EtherAddress;

class CheckCRC32 : public Element {
public:
  CheckCRC32();
  ~CheckCRC32();

  const char *class_name() const		{ return "CheckCRC32"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  CheckCRC32 *clone() const { return(new CheckCRC32()); }
  
  Packet *simple_action(Packet *);

private:

  int _drops;

};

#endif
