/*
 * neighbor.{cc,hh} -- queue element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "neighbor.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ether.h"
#include "elements/standard/scheduleinfo.hh"

Neighbor::Neighbor() : Element(1, 0)
{
}

Neighbor::~Neighbor()
{
}

void *
Neighbor::cast(const char *n)
{
  if (strcmp(n, "Neighbor") == 0)
    return (Neighbor *)this;
  else
    return 0;
}

int
Neighbor::configure(const Vector<String> &, ErrorHandler *)
{
  return 0;
}

int
Neighbor::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
Neighbor::run_scheduled()
{
  if (Packet *p = input(0).pull())
    push(0, p); 
  reschedule();

}

void
Neighbor::push(int, Packet *packet)
{
  assert(packet);
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("Neighbor: got non-Grid packet");
    return;
  }
  eth_ip_pair k(eh->ether_shost, packet->data() + sizeof(click_ether));
  int *num = _addresses.findp(k);
  if (num == 0) {
    // this src addr not already in map, so add it
    _addresses.insert(k, 1);
  }
  else {
    // increment existing count... // XXX why are we even counting. could use bool?
    assert(_addresses.insert(k, *num + 1) == false);
  }

  packet->kill();
}

Neighbor *
Neighbor::clone() const
{
  return new Neighbor;
}

static String
print_nbrs(Element *f, void *)
{
  Neighbor *n = (Neighbor *) f;

  String s = "\nneighbor addrs (";
  s += String(n->_addresses.count());
  s += "):\n";

  int i = 0;
  Neighbor::eth_ip_pair addr;
  int num;
  while (n->_addresses.each(i, addr, num)) {
    s += addr.s();
    s += '\n';
  }
  return s;
}

void
Neighbor::add_handlers()
{
  add_read_handler("nbrs", print_nbrs, 0);
}

EXPORT_ELEMENT(Neighbor)
#include "hashmap.cc"
template class HashMap<Neighbor::eth_ip_pair, int>;
