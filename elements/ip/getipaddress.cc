#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "getipaddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ip.h"

GetIPAddress::GetIPAddress(int offset = 0)
  : Element(1, 1), _offset(offset)
{
}

GetIPAddress *
GetIPAddress::clone() const
{
  return new GetIPAddress(_offset);
}

int
GetIPAddress::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, router, errh,
		     cpUnsigned, "byte offset of IP address", &_offset,
		     0);
}

inline void
GetIPAddress::smaction(Packet *p)
{
  p->set_dst_ip_anno(IPAddress(p->data() + _offset));
}

void
GetIPAddress::push(int, Packet *p)
{
  smaction(p);
  output(0).push(p);
}

Packet *
GetIPAddress::pull(int)
{
  Packet *p = input(0).pull();
  if(p)
    smaction(p);
  return(p);
}

EXPORT_ELEMENT(GetIPAddress)
