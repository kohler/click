/*
 * lrhello.{cc,hh} -- Grid local routing protocol neighbor table broadcast element
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "lrhello.hh"
#include "click_ether.h"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/scheduleinfo.hh"
#include "elements/userlevel/tobpf.hh"
#include "router.hh"
#include "grid.hh"

LocalRouteHello::LocalRouteHello()
  : Element(0, 1), _timer(this), _nbr(0), _hops(1)
{
}

LocalRouteHello::~LocalRouteHello()
{
}

LocalRouteHello *
LocalRouteHello::clone() const
{
  return new LocalRouteHello;
}

int
LocalRouteHello::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "period (msec)", &_period,
			cpInteger, "jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_from_eth,
			cpIPAddress, "source IP address", &_from_ip,
                        cpElement, "Neighbor element", &_nbr,
			cpOptional,
			cpInteger, "max nbr hops", &_hops,
			0);
  if (_period <= 0)
    return errh->error("period must be greater than 0");
  if (_jitter < 0)
    return errh->error("period must be positive");
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  if (_hops < 0)
    return errh->error("max hops must be greater than 0");
  return res;
}

int
LocalRouteHello::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  _timer.attach(this);
  _timer.schedule_after_ms(_period); // Send periodically

  if(_nbr && _nbr->cast("Neighbor") == 0){
    errh->warning("%s: Neighbor argument %s has the wrong type",
                  id().cc(),
                  _nbr->id().cc());
    _nbr = 0;
  } else if (_nbr == 0) {
    errh->warning("%s: no Neighbor element given",
                  id().cc());
  }

  return 0;
}

void
LocalRouteHello::run_scheduled()
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
LocalRouteHello::make_hello()
{
  int psz = sizeof(click_ether) + sizeof(grid_hdr) + sizeof(grid_hello);
  int num_nbrs = 0;
  Vector<grid_nbr_entry> nbrs;
  if (_nbr) {
    _nbr->get_nbrs(&nbrs);
    // don't count nbrs that are too many hops away
    for (int i = 0; i < nbrs.size(); i++) 
      if (nbrs[i].num_hops <= _hops)
	num_nbrs++;
    psz += sizeof(grid_nbr_entry) * num_nbrs;
  }

  WritablePacket *p = Packet::make(psz);
  memset(p->data(), 0, p->length());

  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _from_eth.data(), 6);

  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = psz - sizeof(click_ether);
  gh->type = grid_hdr::GRID_LR_HELLO;
  memcpy(&gh->ip, _from_ip.data(), 4);
  grid_hello *hlo = (grid_hello *) (p->data() + sizeof(click_ether) + sizeof(grid_hdr));
  assert(num_nbrs <= 255);
  hlo->num_nbrs = (unsigned char) num_nbrs;
click_chatter("num_nbrs = %d , _hops = %d, nbrs.size() = %d",
              num_nbrs, _hops, nbrs.size());
  hlo->nbr_entry_sz = sizeof(grid_nbr_entry);

  grid_nbr_entry *curr = (grid_nbr_entry *) (p->data() + sizeof(click_ether) +
					     sizeof(grid_hdr) + sizeof(grid_hello));
  for (int i = 0; i < nbrs.size(); i++) {
    // only include nbrs that are not too many hops away
    if (nbrs[i].num_hops <= _hops) {
      memcpy(curr, &nbrs[i], sizeof(grid_nbr_entry));
      curr++;
    }
  }

  return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LocalRouteHello)

