#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "etherencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

EtherEncap::EtherEncap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

EtherEncap::~EtherEncap()
{
  MOD_DEC_USE_COUNT;
}

EtherEncap *
EtherEncap::clone() const
{
  return new EtherEncap;
}

int
EtherEncap::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned etht;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "Ethernet encapsulation type", &etht,
		  cpEthernetAddress, "source address", &_ethh.ether_shost,
		  cpEthernetAddress, "destination address", &_ethh.ether_dhost,
		  0) < 0)
    return -1;
  if (etht > 0xFFFF)
    return errh->error("argument 1 (Ethernet encapsulation type) must be <= 0xFFFF");
  _ethh.ether_type = htons(etht);
  return 0;
}

Packet *
EtherEncap::smaction(Packet *p)
{
  WritablePacket *q = p->push(14);
  memcpy(q->data(), &_ethh, 14);
  return q;
}

void
EtherEncap::push(int, Packet *p)
{
  output(0).push(smaction(p));
}

Packet *
EtherEncap::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

EXPORT_ELEMENT(EtherEncap)
