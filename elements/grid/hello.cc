/*
 * hello.{cc,hh} -- Grid HELLO broadcast element
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
#include "hello.hh"
#include "click_ether.h"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/scheduleinfo.hh"
#include "elements/userlevel/tobpf.hh"
#include "router.hh"
#include "grid.hh"

Hello::Hello()
  : Element(0, 1), _timer(this)
{
}

Hello::~Hello()
{
}

Hello *
Hello::clone() const
{
  return new Hello;
}

int
Hello::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpInteger, "period (msec)", &_period,
			cpInteger, "jitter (msec)", &_jitter,
			cpEthernetAddress, "source Ethernet address", &_from_eth,
			cpIPAddress, "source IP address", &_from_ip,
			0);
  if (_period <= 0)
    return errh->error("period must be greater than 0");
  if (_jitter < 0)
    return errh->error("period must be positive");
  if (_jitter > _period)
    return errh->error("jitter is bigger than period");
  return res;
}

int
Hello::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  _timer.attach(this);
  _timer.schedule_after_ms(_period); // Send Grid HELLO periodically
  return 0;
}

void
Hello::run_scheduled()
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
Hello::make_hello()
{
  Packet *p = Packet::make(sizeof(click_ether) + sizeof(grid_hdr)); 
  memset(p->data(), 0, p->length());
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _from_eth.data(), 6);
  grid_hdr *gh = (grid_hdr *) (p->data() + sizeof(click_ether));
  gh->hdr_len = sizeof(grid_hdr);
  gh->total_len = sizeof(grid_hdr);
  gh->type = GRID_HELLO;
  memcpy(&gh->ip, _from_ip.data(), 4);
  return p;
}

EXPORT_ELEMENT(Hello)





