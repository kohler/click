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
#include "grid.hh"

Neighbor::Neighbor() : Element(1, 1), _max_hops(3)
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
Neighbor::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
Neighbor::simple_action(Packet *packet)
{
  /*
   * expects grid packets, with MAC hdrs
   */
  assert(packet);
  int jiff = click_jiffies();
  
  /*
   * Update immediate neighbor table with this packet's transmitter's
   * info.
   */
  click_ether *eh = (click_ether *) packet->data();
  if (ntohs(eh->ether_type) != ETHERTYPE_GRID) {
    click_chatter("%s: Neighbor: got non-Grid packet type", id().cc());
    return packet;
  }
  grid_hdr *gh = (grid_hdr *) (packet->data() + sizeof(click_ether));
  IPAddress ipaddr((unsigned char *) &gh->ip);
  EtherAddress ethaddr((unsigned char *) eh->ether_shost);
  NbrEntry *nbr = _addresses.findp(ipaddr);
  if (nbr == 0) {
    // this src addr not already in map, so add it
    NbrEntry new_nbr(ethaddr, ipaddr, jiff);
    _addresses.insert(ipaddr, new_nbr);
    click_chatter("%s: adding %s -- %s", id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
  }
  else {
    // update jiffies and MAC for existing entry
    int old_jiff = nbr->last_updated_jiffies;
    nbr->last_updated_jiffies = jiff;
    if (nbr->eth != ethaddr) {
      if (_timeout_jiffies >= 0 && 
	  (jiff - old_jiff) < _timeout_jiffies) {
	// entry changed before timeout!
	click_chatter("%s: updating %s -- %s", id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
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
  case GRID_LR_HELLO:
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
	for (j = 0; j < _nbrs.size() && curr->ip != _nbrs[j].nbr.ip; j++) 
	  ; // do it
	if (j == _nbrs.size()) {
	  // we don't already know about this nbr
	  _nbrs.push_back(far_entry(jiff, grid_nbr_entry(curr->ip, gh->ip, curr->num_hops + 1)));
	  _nbrs[j].nbr.loc = curr->loc;
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
	    _nbrs[j].last_updated_jiffies = jiff;
	  }
	}
      }
    }
    break;
    
  default:
    break;
  }
  return packet;
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

  String s = "\nimmediate neighbor addrs (";
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

  s += "\nmulti-hop neighbors (";
  s += String(n->_nbrs.size());
  s += "):\n";

  for (int i = 0; i < n->_nbrs.size(); i++) {
    Neighbor::far_entry f = n->_nbrs[i];
    if (n->_timeout_jiffies < 0 || 
	(jiff - f.last_updated_jiffies) < n->_timeout_jiffies) {
      s += IPAddress(f.nbr.ip).s() + " " + IPAddress(f.nbr.next_hop_ip).s() 
	+ " " + String(f.nbr.num_hops) + " " + f.nbr.loc.s() + "\n";
    }
  }

  return s;
}


void
Neighbor::add_handlers()
{
  add_default_handlers(false);
  add_read_handler("nbrs", print_nbrs, 0);
}

bool
Neighbor::get_next_hop(IPAddress dest_ip, EtherAddress *dest_eth) const
{
  assert(dest_eth != 0);
  int jiff = click_jiffies();  

  // is the destination an immediate nbr?
  NbrEntry *ne = _addresses.findp(dest_ip);
  if (ne != 0 && 
      (_timeout_jiffies < 0 || 
       jiff - ne->last_updated_jiffies <= _timeout_jiffies)) {    
    click_chatter("%s: found immediate nbr %s for next hop for %s",
                  id().cc(),
                  ne->ip.s().cc(),
                  dest_ip.s().cc());
    *dest_eth = ne->eth;
    return true;
  }
  if (ne == 0) {
    // not an immediate nbr, search multihop nbrs
    int i;
    for (i = 0; i < _nbrs.size() && dest_ip.addr() != _nbrs[i].nbr.ip; i++)
      ; // do it
    if (i < _nbrs.size() &&
	(_timeout_jiffies < 0 ||
	 jiff - _nbrs[i].last_updated_jiffies <= _timeout_jiffies)) {
      // we know how to get to this dest, look up MAC addr for next hop
      ne = _addresses.findp(IPAddress(_nbrs[i].nbr.next_hop_ip));
      click_chatter("%s: trying to use next hop %s for %s",
                    id().cc(),
                    ne->ip.s().cc(),
                    dest_ip.s().cc());
      if (ne != 0 &&
	  (_timeout_jiffies < 0 ||
	   jiff - ne->last_updated_jiffies <= _timeout_jiffies)) {
	*dest_eth = ne->eth;
	return true;
      }
    }
  }
  return false;
}

void
Neighbor::get_nbrs(Vector<grid_nbr_entry> *retval) const
{
  assert(retval != 0);
  int jiff = click_jiffies();
  for (int i = 0; i < _nbrs.size(); i++) 
    if (_timeout_jiffies < 0 ||
	jiff - _nbrs[i].last_updated_jiffies > _timeout_jiffies)
      retval->push_back(_nbrs[i].nbr);
}


ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(Neighbor)

#include "hashmap.cc"
  template class HashMap<IPAddress, Neighbor::NbrEntry>;

#include "vector.cc"
template class Vector<Neighbor::far_entry>;
template class Vector<grid_nbr_entry>;
