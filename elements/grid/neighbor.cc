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
#include "router.hh"
#include "grid.hh"



Neighbor::Neighbor() : Element(1, 2), _max_hops(3), 
  _hello_timer(hello_hook, (unsigned long) this), 
  _expire_timer(expire_hook, (unsigned long) this),
  _seq_no(0)
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
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "entry timeout (msec)", &_timeout,
			cpInteger, "Hello broadcast period (msec)", &_period,
			cpInteger, "Hello broadcast jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_ethaddr,
			cpIPAddress, "source IP address", &_ipaddr,
			cpOptional,
			cpInteger, "max hops", &_max_hops,
			0);

  // convert msecs to jiffies
  if (_timeout == 0)
    _timeout = -1;
  if (_timeout > 0) {
    _timeout_jiffies = (CLICK_HZ * _timeout) / 1000;
    if (_timeout_jiffies < 1)
      return errh->error("timeout interval is too small");
  }
  else
    click_chatter("%s: not timing out table entries", id().cc());

  if (_period <= 0)
    return errh->error("period must be greater than 0");
  if (_jitter < 0)
    return errh->error("period must be positive");
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  if (_max_hops < 0)
    return errh->error("max hops must be greater than 0");

  return res;
}



int
Neighbor::initialize(ErrorHandler *)
{
  //  ScheduleInfo::join_scheduler(this, errh);
  _hello_timer.attach(this);
  _hello_timer.schedule_after_ms(_period); // Send periodically

  _expire_timer.attach(this);
  if (_timeout > 0)
    _expire_timer.schedule_after_ms(_timeout);

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
    nbr->last_updated_jiffies = jiff;
    if (nbr->eth != ethaddr) 
      click_chatter("%s: updating %s -- %s", id().cc(), ipaddr.s().cc(), ethaddr.s().cc()); 
    nbr->eth = ethaddr;
  }
  
  
  /*
   * perform further packet processing
   */
  switch (gh->type) {
  case grid_hdr::GRID_LR_HELLO:
    {   
      grid_hello *hlo = (grid_hello *) (packet->data() + sizeof(click_ether) + sizeof(grid_hdr));

      /*
       * update far nbr info with this hello sender info -- list sender as
       * its own next hop.
       */
      far_entry *fe = _nbrs.findp(ipaddr);
      if (fe == 0) {
	// we don't already know about it, so add it
	/* XXX not using HashMap2 very efficiently --- fix later */
	_nbrs.insert(ipaddr, far_entry(jiff, grid_nbr_entry(gh->ip, gh->ip, 1, ntohl(hlo->seq_no))));
	fe = _nbrs.findp(ipaddr);
	fe->last_updated_jiffies = jiff;
	fe->nbr.loc = gh->loc;
      } else { 
	// update pre-existing information
	fe->last_updated_jiffies = jiff;
	fe->nbr.num_hops = 1;
	fe->nbr.next_hop_ip = gh->ip;
	fe->nbr.loc = gh->loc;
	fe->nbr.seq_no = ntohl(hlo->seq_no);
      }

      /* 
       * add this sender's nbrs to our far neighbor list.  
       */
      int entry_sz = hlo->nbr_entry_sz;
      // XXX n^2 loop to check if entries are already there
      for (int i = 0; i < hlo->num_nbrs; i++) {
	grid_nbr_entry *curr = (grid_nbr_entry *) (packet->data() + sizeof(click_ether) + 
						   sizeof(grid_hdr) + sizeof(grid_hello) +
						   i * entry_sz);
	if (curr->num_hops + 1 > _max_hops)
	  continue; // skip this one, we don't care about nbrs too many hops away

	if (IPAddress(curr->next_hop_ip) == _ipaddr)
	  continue; // pseduo-split-horizon: ignore routes from nbrs that go back through us
	
	IPAddress curr_ip(curr->ip);
	fe = _nbrs.findp(curr_ip);
	if (fe == 0) {
	  // we don't already know about this nbr
	  _nbrs.insert(curr_ip, far_entry(jiff, grid_nbr_entry(curr->ip, gh->ip, curr->num_hops + 1, ntohl(curr->seq_no))));
	  fe =_nbrs.findp(curr_ip);
	  fe->nbr.loc = curr->loc;
	}
	else { 
	  // replace iff seq_no is newer, or if seq_no is same and hops are less
	  unsigned int curr_seq = ntohl(curr->seq_no);
	  if (curr_seq > fe->nbr.seq_no ||
	      (curr_seq == fe->nbr.seq_no && (curr->num_hops + 1) < fe->nbr.num_hops)) {
	    fe->nbr.num_hops = curr->num_hops + 1;
	    fe->nbr.next_hop_ip = gh->ip;
	    fe->nbr.loc = curr->loc;
	    fe->nbr.seq_no = curr_seq;
	    fe->last_updated_jiffies = jiff;
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
print_nbrs(Element *e, void *)
{
  Neighbor *n = (Neighbor *) e;

  String s = "\nimmediate neighbor addrs (";
  s += String(n->_addresses.count());
  s += "):\n";

  for (Neighbor::Table::Iterator iter = n->_addresses.first(); iter; iter++) {
    s += iter.key().s();
    s += " -- ";
    s += iter.value().eth.s();
    s += '\n';
  }

  s += "\nmulti-hop neighbors (";
  s += String(n->_nbrs.count());
  s += "):\n";
  s += "ip next-hop num-hops loc seq-no\n";

  for (Neighbor::FarTable::Iterator iter = n->_nbrs.first(); iter; iter++) {
    Neighbor::far_entry f = iter.value();
    s += IPAddress(f.nbr.ip).s() + " " + IPAddress(f.nbr.next_hop_ip).s() 
      + " " + String((int) f.nbr.num_hops) + " " + f.nbr.loc.s() 
      + " " + String(f.nbr.seq_no) + "\n";
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

  // is the destination an immediate nbr?
  NbrEntry *ne = _addresses.findp(dest_ip);
  if (ne != 0) {
    click_chatter("%s: found immediate nbr %s for next hop for %s",
                  id().cc(),
                  ne->ip.s().cc(),
                  dest_ip.s().cc());
    *dest_eth = ne->eth;
    return true;
  }
  if (ne == 0) {
    // not an immediate nbr, search multihop nbrs
    far_entry *fe = _nbrs.findp(dest_ip);
    if (fe != 0) {
      // we know how to get to this dest, look up MAC addr for next hop
      ne = _addresses.findp(IPAddress(fe->nbr.next_hop_ip));
      if (ne != 0) {
	*dest_eth = ne->eth;
	click_chatter("%s: trying to use next hop %s for %s",
		      id().cc(),
		      ne->ip.s().cc(),
		      dest_ip.s().cc());
	return true;
      }
      else {
	click_chatter("%s: dude, MAC nbr table and routing table are not consistent!", id().cc());
      }
    }
  }
  return false;
}



void
Neighbor::get_nbrs(Vector<grid_nbr_entry> *retval) const
{
  assert(retval != 0);
  for (Neighbor::FarTable::Iterator iter = _nbrs.first(); iter; iter++)
    retval->push_back(iter.value().nbr);
}


void
Neighbor::expire_hook(unsigned long thunk) 
{
  Neighbor *n = (Neighbor *) thunk;
  n->expire_routes();
  n->_expire_timer.schedule_after_ms(n->_timeout);
}


void
Neighbor::expire_routes()
{
  assert(_timeout > 0);
  int jiff = click_jiffies();

  // XXX not sure if we are allowed to iterate while modifying map
  // (i.e. erasing entries), so figure out what to expire first.
  Vector<IPAddress> expired_addresses;
  Vector<IPAddress> expired_nbrs;

  // find the expired entries
  for (Neighbor::Table::Iterator iter = _addresses.first(); iter; iter++) 
    if (jiff - iter.value().last_updated_jiffies > _timeout_jiffies)
      expired_addresses.push_back(iter.key());
  for (Neighbor::FarTable::Iterator iter = _nbrs.first(); iter; iter++) 
    if (jiff - iter.value().last_updated_jiffies > _timeout_jiffies)
      expired_nbrs.push_back(iter.key());
    
  // remove expired entries
  for (int i = 0; i < expired_addresses.size(); i++) {
    click_chatter("%s: expiring address for %s", id().cc(), expired_addresses[i].s().cc());
    assert(_addresses.remove(expired_addresses[i]));
  }
  for (int i = 0; i < expired_nbrs.size(); i++) {
    click_chatter("%s: expiring route entry for %s", id().cc(), expired_nbrs[i].s().cc());
    assert(_nbrs.remove(expired_nbrs[i]));
  }
}



void
Neighbor::hello_hook(unsigned long thunk)
{
  Neighbor *n = (Neighbor *) thunk;

  n->output(1).push(n->make_hello());

  // XXX this random stuff is not right i think... wouldn't it be nice
  // if click had a phat RNG like ns?
  int r2 = random();
  double r = (double) (r2 >> 1);
  int jitter = (int) (((double) n->_jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  n->_hello_timer.schedule_after_ms(n->_period + (int) jitter);
}



Packet *
Neighbor::make_hello()
{
  int psz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  int num_nbrs =_nbrs.count();

  psz += sizeof(grid_nbr_entry) * num_nbrs;

  WritablePacket *p = Packet::make(psz);
  memset(p->data(), 0, p->length());

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _ethaddr.data(), 6);

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = psz - sizeof(click_ether);
  gh->total_len = htons(gh->total_len);
  gh->type = grid_hdr::GRID_LR_HELLO;
  memcpy(&gh->ip, _ipaddr.data(), 4);

  grid_hello *hlo = (grid_hello *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));
  assert(num_nbrs <= 255);
  hlo->num_nbrs = (unsigned char) num_nbrs;
#if 1
  click_chatter("num_nbrs = %d , _hops = %d, nbrs.count() = %d",
		num_nbrs, _max_hops, _nbrs.count());
#endif
  hlo->nbr_entry_sz = sizeof(grid_nbr_entry);
  hlo->seq_no = htonl(_seq_no);
  /* originating sequence numbers are even, starting at 0.  odd
     numbers are reserved for other nodes to advertise a broken route
     to us.  from DSDV paper. */
  _seq_no += 2;

  grid_nbr_entry *curr = (grid_nbr_entry *) (p->data() + sizeof(click_ether) +
					     sizeof(grid_hdr) + sizeof(grid_hello));
  for (Neighbor::FarTable::Iterator iter = _nbrs.first(); iter; iter++) {
    /* XXX if everyone is using the same max-hops parameter, we could
       leave out all of our entries that are exactly max-hops hops
       away, because we know those entries will be greater than
       max-hops at any neighbor.  but, let's leave it in case we have
       different max-hops across the network */
    memcpy(curr, &iter.value().nbr, sizeof(grid_nbr_entry));
    curr->seq_no = htonl(curr->seq_no);
    curr++;
  }

  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(Neighbor)

#include "bighashmap.cc"
template class BigHashMap<IPAddress, Neighbor::NbrEntry>;
template class BigHashMap<IPAddress, Neighbor::far_entry>;
#include "vector.cc"
template class Vector<IPAddress>;
template class Vector<grid_nbr_entry>;
