#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "etheraddress.hh"
#include "glue.hh"

EtherAddress::EtherAddress(unsigned char *addr)
{
  memcpy(data(), addr, 6);
}

bool
EtherAddress::is_group() {
  return ((char*)_data)[0] & 1;
}

String
EtherAddress::s() const {
  char buf[20];
  const unsigned char *p = this->data();

  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
	  p[0], p[1], p[2], p[3], p[4], p[5]);

  return buf;
}
