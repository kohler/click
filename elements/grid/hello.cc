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
Hello::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  if(args.size() != 2){
    errh->error("usage: Hello(T, eth)");
    return(-1);
  }
  
  if (cp_integer(args[0], _period) &&
      cp_ethernet_address(args[1], _from)){
    /* yow */
  } else {
    errh->error("Hello configuration expected integer period");
    return -1;
  }
  
  return 0;
}

int
Hello::initialize(ErrorHandler *errh)
{
  ScheduleInfo::join_scheduler(this, errh);
  _timer.attach(this);
  _timer.schedule_after_ms(_period * 1000); // Send an ARP reply periodically.
  return 0;
}

void
Hello::run_scheduled()
{
  output(0).push(make_hello());
  _timer.schedule_after_ms(_period * 1000);
  click_chatter("hey ho let's go");
}

Packet *
Hello::make_hello()
{
  Packet *p = Packet::make(sizeof(click_ether));
  memset(p->data(), 0, p->length());
  click_ether *eh = (click_ether *) p->data();
  memset(eh->ether_dhost, 0xff, 6);
  eh->ether_type = htons(ETHERTYPE_GRID);
  memcpy(eh->ether_shost, _from.data(), 6);
  return p;
}

EXPORT_ELEMENT(Hello)





