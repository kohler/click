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
  return cp_va_parse(conf, this, errh,
		     cpInteger, "period (sec)", &_period,
		     cpEthernetAddress, "source Ethernet address", &_from_eth,
		     cpIPAddress, "source IP address", &_from_ip,
		     0);
}

int
Hello::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  _timer.attach(this);
  _timer.schedule_after_ms(_period * 1000); // Send Grid HELLO periodically
  return 0;
}

void
Hello::run_scheduled()
{
  output(0).push(make_hello());
  _timer.schedule_after_ms(_period * 1000);
}

Packet *
Hello::make_hello()
{
  Packet *p = Packet::make(sizeof(click_ether) + 4); // XXX 
  memset(p->data(), 0, p->length());
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6); // broadcast
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _from_eth.data(), 6);
  memcpy(p->data() + sizeof(click_ether), _from_ip.data(), 4);
  return p;
}

EXPORT_ELEMENT(Hello)





