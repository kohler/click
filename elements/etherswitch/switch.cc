#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "switch.hh"
#include "click_ether.h"
#include "etheraddress.hh"
#include "glue.hh"
#include "bitvector.hh"

EtherSwitch::AddrInfo::AddrInfo(int p, const timeval& s)
  : port(p), stamp(s)
{
}

EtherSwitch::EtherSwitch()
  : _table(0), _timeout(300)
{
}


EtherSwitch::~EtherSwitch()
{
}

Bitvector
EtherSwitch::forward_flow(int i) const
{
  Bitvector bv(noutputs(), i >= 0 && i < ninputs());
  if (i >= 0 && i < noutputs()) bv[i] = false;
  return bv;
}

Bitvector
EtherSwitch::backward_flow(int o) const
{
  Bitvector bv(ninputs(), o >= 0 && o < noutputs());
  if (o >= 0 && o < ninputs()) bv[o] = false;
  return bv;
}


EtherSwitch*
EtherSwitch::clone() const
{
  return new EtherSwitch;
}

void
EtherSwitch::broadcast(int source, Packet *p)
{
  int n = noutputs();
  int sent = 0;
  for (int i = 0; i < n; i++)
    if (i != source) {
      Packet *pp = (sent < n - 2 ? p->clone() : p);
      output(i).push(pp);
      sent++;
    }
}

void
EtherSwitch::push(int source, Packet *p)
{
  ether_header* e = (ether_header*) p->data();

  timeval t;
  click_gettimeofday(&t);

  EtherAddress src = EtherAddress(e->ether_shost);
  EtherAddress dst = EtherAddress(e->ether_dhost);

#if 0
  click_chatter("Got a packet %p on %d at %d.%06d with src %s and dst %s",
	      p, source, t.tv_sec, t.tv_usec,
              src.s().cc(),
              dst.s().cc());
#endif

  if (AddrInfo* src_info = _table[src]) {
    src_info->port = source;	// It's possible that it has changed.
    src_info->stamp = t;
  } else {
    _table.insert(src, new AddrInfo(source, t));
  }
  
  int outport = -1;		// Broadcast
  
  // Set outport if dst is unicast, we have info about it, and the
  // info is still valid.
  if (!dst.is_group()) {
    if (AddrInfo* dst_info = _table[dst]) {
      //      click_chatter("Got a packet for a known dst on %d to %d\n",
      //		  source, dst_info->port);
      t.tv_sec -= _timeout;
      if (timercmp(&dst_info->stamp, &t, >)) {
	outport = dst_info->port;
      }
    }
  }

  if (outport < 0)
    broadcast(source, p);
  else if (outport == source)	// Don't send back out on same interface
    p->kill();
  else				// forward
    output(outport).push(p);
}

String
EtherSwitch::read_table(Element* f, void *) {
  EtherSwitch* sw = (EtherSwitch*)f;
  String s;
  EtherAddress ea;
  AddrInfo* ai;

  int i = 0;
  while (sw->_table.each(i, ea, ai)) {
    s += ea.s() + " " + String(ai->port) + "\n";
  }
  return s;
}

void
EtherSwitch::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("table", read_table, 0);
}

EXPORT_ELEMENT(EtherSwitch)

#include "hashmap.cc"
template class HashMap<EtherAddress, EtherSwitch::AddrInfo*>;
