/*
 * neighbor.{cc,hh} -- Grid local neighbor table element
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
Neighbor::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int msec_timeout = 0;
  int res = cp_va_parse(conf, this, errh,
		     cpInteger, "entry timeout (msec)", &msec_timeout,
		     cpEthernetAddress, "source Ethernet address", &_ethaddr,
		     cpIPAddress, "source IP address", &_ipaddr,
		     0);

  // convert msecs to jiffies
  if (msec_timeout < 0) {
    _timeout_jiffies = (CLICK_HZ * msec_timeout) / 1000;
    if (_timeout_jiffies < 1) 
      return errh->error("timeout interval is too small");
  }
  else // never timeout
    _timeout_jiffies = -1;
  return res;
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
  int jiff = click_jiffies();
  
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
    EtherAddress ethaddr((unsigned char *) eh->ether_shost);
    NbrEntry *nbr = _addresses.findp(ipaddr);
    if (nbr == 0) {
      // this src addr not already in map, so add it
      NbrEntry new_nbr(ethaddr, ipaddr, jiff);
      _addresses.insert(ipaddr, new_nbr);
      click_chatter("adding %s -- %s", ipaddr.s().cc(), ethaddr.s().cc()); 
    }
    else {
      // update jiffies and MAC for existing entry
      int old_jiff = nbr->last_updated_jiffies;
      nbr->last_updated_jiffies = jiff;
      if (nbr->eth != ethaddr) {
	if ((jiff - old_jiff) < _timeout_jiffies) {
	  // entry changed before timeout!
	  click_chatter("updating %s -- %s", ipaddr.s().cc(), ethaddr.s().cc()); 
	}
	nbr->eth = ethaddr;
      }
    }

    // perform further packet processing
    switch (gh->type) {
    case GRID_HELLO:
      // nothing further to do
      packet->kill();
      break;
    case GRID_NBR_ENCAP:
      // XXX do we need to annotate the packet??  
      // check actual header length in case versions differ
      packet->pull(sizeof(click_ether) + gh->hdr_len); 
      output(1).push(packet);
      break;
    default:
      click_chatter("Neighbor: received unknown Grid packet: %d", (int) gh->type);
    }
  }
  else {
    /*
     * input from higher level protocol -- expects IP packets
     * annotated with src IP address
     */
    assert(port == 1);
    // check to see is the desired dest is our neighbor
    IPAddress dst = packet->dst_ip_anno();
    NbrEntry *nbr = _addresses.findp(dst);
    if (nbr == 0) {
      click_chatter("Neighbor: dropping packet for unknown destination: %s", dst.s().cc());
      packet->kill(); // too bad!
    } 
    else if (_timeout_jiffies > 0 && 
	     (jiff - nbr->last_updated_jiffies) > _timeout_jiffies) {
      // XXX would like to remove entry from HashMap so it doesn't
      // grow too big, but don't know how!

      // click_chatter("Neighbor: dropping packet for destination with stale entry: %s", dst.s().cc());
      packet->kill(); 
    }
    else {
      // encapsulate packet with grid hdr and send it out!
      packet = packet->push(sizeof(click_ether) + sizeof(grid_hdr));
      bzero(packet->data(), sizeof(click_ether) + sizeof(grid_hdr));
      
      click_ether *eh = (click_ether *) packet->data();
      memcpy(eh->ether_dhost, nbr->eth.data(), 6);
      memcpy(eh->ether_shost, _ethaddr.data(), 6);
      eh->ether_type = htons(ETHERTYPE_GRID);
      
      grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
      gh->hdr_len = sizeof(grid_hdr);
      gh->total_len = sizeof(grid_hdr);
      gh->type = GRID_NBR_ENCAP;
      memcpy((unsigned char *) &gh->ip, _ipaddr.data(), 4);
      
      output(0).push(packet);
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
  int jiff = click_jiffies();
  Neighbor *n = (Neighbor *) f;

  String s = "\nneighbor addrs (";
  s += String(n->_addresses.count());
  s += "):\n";

  int i = 0;
  IPAddress ipaddr;
  Neighbor::NbrEntry nbr;
  while (n->_addresses.each(i, ipaddr, nbr)) {
    if (n->_timeout_jiffies < 0 || 
	(jiff - nbr.last_updated_jiffies) < n->_timeout_jiffies) {
      s += ipaddr.s();
      s += " -- ";
      s += nbr.eth.s();
      s += '\n';
    }
  }
  return s;
}

void
Neighbor::add_handlers()
{
  add_read_handler("nbrs", print_nbrs, 0);
}

bool
Neighbor::knows_about(IPAddress nbr)
{
  int jiff = click_jiffies();
  NbrEntry *n = _addresses.findp(nbr);
  if (n == 0) 
    return false;
  else // check if timed out!
    return (jiff - n->last_updated_jiffies) < _timeout_jiffies;
}


EXPORT_ELEMENT(Neighbor)

#include "hashmap.cc"
  template class HashMap<IPAddress, Neighbor::NbrEntry>;

