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

Neighbor::Neighbor() : Element(2, 2), _max_hops(1)
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
			cpOptional,
			cpInteger, "max hops", &_max_hops,
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

    /*
     * update immediate neighbor table
     */
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

    /*
     * update far nbr info with this hello sender info.
     */
    int i;
    for (i = 0; i < _nbrs.size() && gh->ip != _nbrs[i].nbr.ip; i++) 
      ; // do it
    if (i == _nbrs.size()) {
      // we don't already know about it, so add it
      _nbrs.push_back(far_entry(jiff, grid_nbr_entry(gh->ip, gh->ip, 1)));
      _nbrs[i].last_updated_jiffies = jiff;
      _nbrs[i].nbr.loc = gh->loc;
      _nbrs[i].nbr.loc.ntohloc();
    }
    else { 
      // update pre-existing information
      // XXX simply replace information, or only replace expired info?
      // for now we will replace info if expired, or if nbr is now closer.
      if ((jiff - _nbrs[i].last_updated_jiffies > _timeout_jiffies) ||
	  (_nbrs[i].nbr.num_hops > 1)) {
	_nbrs[i].last_updated_jiffies = jiff;
	_nbrs[i].nbr.num_hops = 1;
	_nbrs[i].nbr.next_hop_ip = gh->ip;
	_nbrs[i].nbr.loc = gh->loc;
	_nbrs[i].nbr.loc.ntohloc();
      }
    }
     
    /*
     * XXX when do we actually remove expired nbr info, as opposed to
     * just ignore it? eventually we will run out of space!
     */

    /*
     * perform further packet processing
     */
    switch (gh->type) {
    case GRID_HELLO:
      {    /* 
	    * add this sender's nbrs to our far neighbor list.  
	    */
	grid_hello *hlo = (grid_hello *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));
	int entry_sz = hlo->nbr_entry_sz;
	// XXX n^2 loop to check if entries are already there
	for (int i = 0; i < hlo->num_nbrs; i++) {
	  grid_nbr_entry *curr = (grid_nbr_entry *) (packet->data() + sizeof(click_ether) + 
						     sizeof(grid_hdr) + sizeof(grid_hello) +
						     i * entry_sz);
	  if (curr->num_hops + 1 > _max_hops)
	    continue; // skip this one, we don't care about nbrs too many hops away
	  
	  int j;
	  for (j = 0; j < _nbrs.size() && curr->ip != _nbrs[i].nbr.ip; i++) 
	    ; // do it
	  if (j == _nbrs.size()) {
	    // we don't already know about this nbr
	    _nbrs.push_back(far_entry(jiff, grid_nbr_entry(curr->ip, gh->ip, curr->num_hops + 1)));
	    _nbrs[j].nbr.loc = curr->loc;
	    _nbrs[j].nbr.loc.ntohloc();
	    _nbrs[j].last_updated_jiffies = jiff;
	  }
	  else { 
	    // update pre-existing information.  use same criteria as when
	    // updating pre-existing information of 1-hop nbr.
	    if ((jiff - _nbrs[j].last_updated_jiffies > _timeout_jiffies) ||
		(_nbrs[j].nbr.num_hops > curr->num_hops)) {
	      _nbrs[j].nbr.num_hops = curr->num_hops + 1;
	      _nbrs[j].nbr.next_hop_ip = gh->ip;
	      _nbrs[j].nbr.loc = curr->loc;
	      _nbrs[j].nbr.loc.ntohloc();
	      _nbrs[j].last_updated_jiffies = jiff;
	    }
	  }
	}
	
	// nothing further to do
	packet->kill();
      }
      break;

    case GRID_NBR_ENCAP:
      {
	/* 
	 * try to either receive the packet or forward it
	 */
	struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));
	IPAddress dest_ip(encap->dst_ip);
	if (dest_ip == _ipaddr) {
	  // it's for us, send to higher level
	  packet->pull(sizeof(click_ether) + gh->hdr_len + sizeof(grid_nbr_encap)); 
	  output(1).push(packet);
	  break;
	}
	else {
	  // try to forward it!
	  forward_grid_packet(packet, encap->dst_ip);
	}
      }
      break;

    default:
      click_chatter("%s: received unknown Grid packet: %d", id().cc(), (int) gh->type);
      packet->kill();
    }
  }
  else {
    /*
     * input from higher level protocol -- expects IP packets
     * annotated with src IP address.
     */
    assert(port == 1);
    // check to see is the desired dest is our neighbor
    IPAddress dst = packet->dst_ip_anno();
    if (dst == _ipaddr) {
      click_chatter("%s: got ip packet for our address; looping it back", id().cc());
      output(1).push(packet);
    }
    else {
      // encapsulate packet with grid hdr and try to send it out
      int old_len = packet->length();
      packet = packet->push(sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap));
      memset(packet->data(), 0, sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap));

      struct click_ether *eh = (click_ether *) packet->data();
      eh->ether_type = htons(ETHERTYPE_GRID);

      struct grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
      gh->hdr_len = sizeof(grid_hdr);
      gh->total_len = sizeof(grid_hdr) + sizeof(grid_nbr_encap) + old_len;
      gh->type = GRID_NBR_ENCAP;

      struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));
      encap->hops_travelled = 0;
      encap->dst_ip = dst.addr();

      forward_grid_packet(packet, dst);
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
Neighbor::get_nbrs(Vector<grid_nbr_entry> &retval) const
{
  int jiff = click_jiffies();
  for (int i = 0; i < _nbrs.size(); i++) 
    if (jiff - _nbrs[i].last_updated_jiffies > _timeout_jiffies)
      retval.push_back(_nbrs[i].nbr);
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


void
Neighbor::forward_grid_packet(Packet *packet, IPAddress dest_ip)
{
  /*
   * packet must have a MAC hdr, grid_hdr, and a grid_nbr_encap hdr on
   * it.  This function will update the hop count, src ip (us) (we
   * don't have distinction between hops and src (XXX)) and dst/src
   * MAC addresses.  
   */

  click_chatter("fwding for dst %s", dest_ip.s().cc());

  int jiff = click_jiffies();
  struct grid_nbr_encap *encap = (grid_nbr_encap *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));

  // is the destination an immediate nbr?
  EtherAddress *next_hop_eth = 0;
  NbrEntry *ne = _addresses.findp(dest_ip);
  if (ne != 0 && 
      ne->last_updated_jiffies - jiff <= _timeout_jiffies) {
    click_chatter("found immediate nbr: %s", ne->ip.s().cc());
    next_hop_eth = &(ne->eth);
  }
  if (next_hop_eth == 0) {
    // not an immediate nbr, search multihop nbrs
    int i;
    for (i = 0; i < _nbrs.size() && dest_ip.addr() != _nbrs[i].nbr.ip; i++)
      ; // do it
    if (i < _nbrs.size() &&
	jiff - _nbrs[i].last_updated_jiffies <= _timeout_jiffies) {
      // we know how to get to this dest, look up MAC addr for next hop
      ne = _addresses.findp(IPAddress(_nbrs[i].nbr.next_hop_ip));
      click_chatter("trying to use next hop %s", ne->ip.s().cc());
      if (ne != 0 &&
	  ne->last_updated_jiffies - jiff <= _timeout_jiffies)
	next_hop_eth = &(ne->eth);
    }
  }
  
  if (next_hop_eth != 0) {
    struct click_ether *eh = (click_ether *) packet->data();
    memcpy(eh->ether_shost, _ethaddr.data(), 6);
    memcpy(eh->ether_dhost, next_hop_eth->data(), 6);
    struct grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
    gh->ip = _ipaddr.addr();
    encap->hops_travelled++;
    // leave src location update to FixSrcLoc element
    output(0).push(packet);
  }
  else {
    click_chatter("%s: unable to forward packet for %s", id().cc(), dest_ip.s().cc());
    packet->kill();
  }
}

EXPORT_ELEMENT(Neighbor)

#include "hashmap.cc"
  template class HashMap<IPAddress, Neighbor::NbrEntry>;

#include "vector.cc"
template class Vector<Neighbor::far_entry>;
template class Vector<grid_nbr_entry>;
