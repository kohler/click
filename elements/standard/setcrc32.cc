#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "setcrc32.hh"

extern "C" {
#include "crc32.h"
}

SetCRC32::SetCRC32()
{
  add_input();
  add_output();
}

SetCRC32::~SetCRC32()
{
}

Packet *
SetCRC32::simple_action(Packet *p)
{
  int len = p->length();
  unsigned int crc = 0xffffffff;
  crc = update_crc(crc, (char *) p->data(), len);

  Packet *q = p->put(4);
  memcpy(q->data() + len, &crc, 4);
  
  return(q);
}

EXPORT_ELEMENT(SetCRC32)
