#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "setipaddress.hh"
#include "confparse.hh"
#include <string.h>

SetIPAddress::SetIPAddress(unsigned offset = 0)
  : Element(1, 1), _offset(offset)
{
}

Packet *
SetIPAddress::simple_action(Packet *p)
{
  IPAddress ipa = p->dst_ip_anno();
  if (ipa && _offset + 4 <= p->length())
    memcpy(p->data() + _offset, &ipa, 4);
  // XXX error reporting?
  return p;
}

SetIPAddress *
SetIPAddress::clone() const
{
  return new SetIPAddress(_offset);
}

int
SetIPAddress::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, router, errh,
		     cpUnsigned, "byte offset of IP address", &_offset,
		     0);
}

EXPORT_ELEMENT(SetIPAddress)
