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
  if (WritablePacket *q = p->push(14)) {
    memcpy(q->data(), &_ethh, 14);
    return q;
  } else
    return 0;
}

void
EtherEncap::push(int, Packet *p)
{
  if (Packet *q = smaction(p))
    output(0).push(q);
}

Packet *
EtherEncap::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

String
EtherEncap::read_handler(Element *e, void *thunk)
{
  EtherEncap *ee = static_cast<EtherEncap *>(e);
  switch ((int)thunk) {
   case 0:	return EtherAddress(ee->_ethh.ether_shost).s() + "\n";
   case 1:	return EtherAddress(ee->_ethh.ether_dhost).s() + "\n";
   default:	return "<error>\n";
  }
}

void
EtherEncap::add_handlers()
{
  add_read_handler("src", read_handler, (void *)0);
  add_write_handler("src", reconfigure_positional_handler, (void *)1);
  add_read_handler("dst", read_handler, (void *)1);
  add_write_handler("dst", reconfigure_positional_handler, (void *)2);
}

EXPORT_ELEMENT(EtherEncap)
