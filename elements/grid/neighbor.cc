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

#include <stddef.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "neighbor.hh"
#include "confparse.hh"
#include "error.hh"
#include "click_ether.h"
#include "click_ip.h"
#include "elements/standard/scheduleinfo.hh"
#include "grid.hh"

Neighbor::Neighbor() : Element(2, 2)
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
Neighbor::push(int port, Packet *packet)
{
  /* 
   * input 1 and output 1 hook up to higher level (e.g. ip), input 0
   * and output 0 hook up to lower level (e.g. ethernet) 
   */

  assert(packet);
  if (port == 0) {
    /*
     * input from net device
     */

    // update neighbor table
    click_ether *eh = (click_ether *) packet->data();
    if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
      click_chatter("Neighbor: got non-Grid packet");
      return;
    }
    grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
    IPAddress ipaddr((unsigned char *) &gh->ip);
    EtherAddress *ethaddr = _addresses.findp(ipaddr);
    if (ethaddr == 0) {
      // this src addr not already in map, so add it
      EtherAddress ea((unsigned char *) eh->ether_shost);
      _addresses.insert(ipaddr, ea);
      click_chatter("adding %s -- %s", ipaddr.s().cc(), ea.s().cc()); 
    }

    // perform further packet processing
    switch (gh->type) {
    case GRID_HELLO:
      click_chatter("got hello");
      // nothing further to do
      packet->kill();
      break;
    case GRID_NBR_ENCAP:
      // XXX do we need to annotate the packet??
      packet->pull(sizeof(click_ether) + sizeof(grid_hdr));
      output(1).push(packet);
      break;
    default:
      click_chatter("Neighbor: received unknown Grid packet: %d", (int) gh->type);
    }
  }
  else {
    /*
     * input from higher level protocol -- expects IP packets
     */
    assert(port == 1);
    // check to see is the desired dest is our neighbor
    IPAddress dst(packet->data() + offsetof(click_ip, ip_dst));
    EtherAddress *ethaddr = _addresses.findp(dst);
    if (ethaddr == 0) {
      click_chatter("Neighbor: dropping packet for %s", dst.s().cc());
      packet->kill(); // too bad!
    }
    else {
      // encapsulate packet with grid hdr and send it out!
      packet->push(sizeof(click_ether) + sizeof(grid_hdr));
      bzero(packet->data(), sizeof(click_ether) + sizeof(grid_hdr));
      
      click_ether *eh = (click_ether *) packet->data();
      memcpy(eh->ether_dhost, ethaddr->data(), 6);
      memcpy(eh->ether_shost, _ethaddr.data(), 6);
      eh->ether_type = htons(ETHERTYPE_GRID);
      
      grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
      gh->len = sizeof(grid_hdr);
      gh->type = GRID_NBR_ENCAP;
      memcpy((unsigned char *) &gh->ip, _ipaddr.data(), 4);
      output(1).push(packet);
    }
  }
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
  IPAddress ipaddr;
  EtherAddress ethaddr;
  while (n->_addresses.each(i, ipaddr, ethaddr)) {
    s += ipaddr.s();
    s += " -- ";
    s += ethaddr.s();
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
  // below already in lib/templatei.cc
  // #include "hashmap.cc"
  // template class HashMap<IPAddress, EtherAddress>;
