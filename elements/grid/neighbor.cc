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

Neighbor::Neighbor() : Element(1, 2), _max_hops(3), _timer(this), _seq_no(0)
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
			cpInteger, "Hello broadcast period (msec)", &_period,
			cpInteger, "Hello broadcast jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_ethaddr,
			cpIPAddress, "source IP address", &_ipaddr,
			cpOptional,
			cpInteger, "max hops", &_max_hops,
			0);

  // convert msecs to jiffies
  if (msec_timeout > 0) {
    _timeout_jiffies = (CLICK_HZ * msec_timeout) / 1000;
    if (_timeout_jiffies < 1) 
      return errh->error("timeout interval is too small");
  }
  else // never timeout
    _timeout_jiffies = -1;

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
Neighbor::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  _timer.attach(this);
  _timer.schedule_after_ms(_period); // Send periodically

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
   * update far nbr info with this hello sender info -- list sender as
   * its own next hop.
   */
  int i;
  for (i = 0; i < _nbrs.size() && gh->ip != _nbrs[i].nbr.ip; i++) 
    ; // do it
  if (i == _nbrs.size()) {
    // we don't already know about it, so add it

    // XXX shit how to deal with seq num????? maybe don't mix
    // immediate nbrs info with routing table info?

    _nbrs.push_back(far_entry(jiff, grid_nbr_entry(gh->ip, gh->ip, 1, 0)));
    _nbrs[i].last_updated_jiffies = jiff;
    _nbrs[i].nbr.loc = gh->loc;
  } else { 
    // update pre-existing information
    _nbrs[i].last_updated_jiffies = jiff;
    _nbrs[i].nbr.num_hops = 1;
    _nbrs[i].nbr.next_hop_ip = gh->ip;
    _nbrs[i].nbr.loc = gh->loc;
  }
  
  /*
   * XXX when do we actually remove expired nbr info, as opposed to
   * just ignore it? eventually we will run out of space!
   */
  
  /*
   * perform further packet processing
   */
  switch (gh->type) {
  case grid_hdr::GRID_LR_HELLO:
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

	if (IPAddress(curr->next_hop_ip) == _ipaddr)
	  continue; // pseduo-split-horizon: ignore routes from nbrs that go back through us
	
	int j;
	for (j = 0; j < _nbrs.size() && curr->ip != _nbrs[j].nbr.ip; j++) 
	  ; // do it
	if (j == _nbrs.size()) {
	  // we don't already know about this nbr
	  _nbrs.push_back(far_entry(jiff, grid_nbr_entry(curr->ip, gh->ip, 
							 curr->num_hops + 1, ntohl(curr->seq_no))));
	  _nbrs[j].nbr.loc = curr->loc;
	  _nbrs[j].last_updated_jiffies = jiff;
	}
	else { 
	  // update pre-existing information.  replace all information
	  // if next hop is the same as in the old entry; if the next
	  // hop is different, only update if old info is timed out or
	  // more hops away.

	  // XXX check seq num stuff!!

	  if (_nbrs[j].nbr.next_hop_ip == gh->ip ||
	      jiff - _nbrs[j].last_updated_jiffies > _timeout_jiffies ||
	      _nbrs[j].nbr.num_hops > curr->num_hops) {
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

  for (Neighbor::Table::Iterator iter = n->_addresses.first(); iter; iter++) {
    if (n->_timeout_jiffies < 0 || 
	(jiff - iter.value().last_updated_jiffies) < n->_timeout_jiffies) {
      s += iter.key().s();
      s += " -- ";
      s += iter.value().eth.s();
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
    if ((_timeout_jiffies < 0 ||
	 jiff - _nbrs[i].last_updated_jiffies < _timeout_jiffies) &&
	_nbrs[i].nbr.num_hops <= _max_hops)
      retval->push_back(_nbrs[i].nbr);
}


void
Neighbor::run_scheduled()
{
  output(0).push(make_hello());

  // XXX this random stuff is not right i think... wouldn't it be nice
  // if click had a phat RNG like ns?
  int r2 = random();
  double r = (double) (r2 >> 1);
  int  jitter = (int) (((double) _jitter) * r / ((double) 0x7FffFFff));
  if (r2 & 1)
    jitter *= -1;
  _timer.schedule_after_ms(_period + (int) jitter);
}

Packet *
Neighbor::make_hello()
{
  int psz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  int num_nbrs = 0;
  Vector<grid_nbr_entry> nbrs;
  get_nbrs(&nbrs);

  psz += sizeof(grid_nbr_entry) * nbrs.size();

  WritablePacket *p = Packet::make(psz);
  memset(p->data(), 0, p->length());

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _ethaddr.data(), 6);

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = psz - sizeof(click_ether);
  gh->type = grid_hdr::GRID_LR_HELLO;
  memcpy(&gh->ip, _ipaddr.data(), 4);

  grid_hello *hlo = (grid_hello *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));
  assert(num_nbrs <= 255);
  hlo->num_nbrs = (unsigned char) num_nbrs;
#if 1
  click_chatter("num_nbrs = %d , _hops = %d, nbrs.size() = %d",
		num_nbrs, _max_hops, nbrs.size());
#endif
  hlo->nbr_entry_sz = sizeof(grid_nbr_entry);
  hlo->seq_no = htonl(_seq_no);
  _seq_no += 2;

  grid_nbr_entry *curr = (grid_nbr_entry *) (p->data() + sizeof(click_ether) +
					     sizeof(grid_hdr) + sizeof(grid_hello));
  for (int i = 0; i < nbrs.size(); i++) {
    // only include nbrs that are not too many hops away
    if (nbrs[i].num_hops <= _max_hops) {
      memcpy(curr, &nbrs[i], sizeof(grid_nbr_entry));
      curr->seq_no = htonl(curr->seq_no);
      curr++;
    }
  }

  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(Neighbor)

#include "hashmap.cc"
  template class HashMap<IPAddress, Neighbor::NbrEntry>;

#include "vector.cc"
template class Vector<Neighbor::far_entry>;
template class Vector<grid_nbr_entry>;
