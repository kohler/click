#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "etherencap.hh"
#include "etheraddress.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

EtherEncap::EtherEncap()
  : Element(1, 1), _type(-1)
{
}

EtherEncap::~EtherEncap()
{
}

EtherEncap *
EtherEncap::clone() const
{
  return new EtherEncap;
}

int
EtherEncap::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "Ethernet encapsulation type", &_type,
		  cpEthernetAddress, "source Ethernet address", &_src,
		  cpEthernetAddress, "destination Ethernet address", &_dst,
		  0) < 0)
    return -1;
  if (_type > 0xFFFF)
    return errh->error("argument 1 (Ethernet encapsulation type) must be <= 0xFFFF");
  return 0;
}

int
EtherEncap::initialize(ErrorHandler *errh)
{
  if (_type < 0)
    return errh->error("not configured");
  _netorder_type = htons(_type);
  return 0;
}

Packet *
EtherEncap::simple_action(Packet *p)
{
  Packet *q = p->push(14);
  
  memcpy(q->data(), _dst, 6);
  memcpy(q->data() + 6, _src, 6);
  memcpy(q->data() + 12, &_netorder_type, 2);
  
  return(q);
}

EXPORT_ELEMENT(EtherEncap)
