/*
 * collisiondetection.{cc,hh} -- extract per-packet link tx counts
 * John Bicket
 *
 * Copyright (c) 1999-2002 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/collisiondetection.hh>
CLICK_DECLS


#define max(a, b) ((a) > (b) ? (a) : (b))


CollisionDetection::CollisionDetection()
  : Element(1,1),
    _timer(this),
    _p(0),
    _collisions(0) 
{
  MOD_INC_USE_COUNT;

  click_gettimeofday(&_busy_until);
}

CollisionDetection::~CollisionDetection()
{
  if (_p) {
    _p->kill();
    _p = 0;
  }
  MOD_DEC_USE_COUNT;
}

int
CollisionDetection::initialize (ErrorHandler *)
{
  _timer.initialize (this);
  return 0;
}

int
CollisionDetection::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			cpEnd);
  return res;
  

}


void 
CollisionDetection::run_timer() 
{
  struct timeval now;
  click_gettimeofday(&now);
  if (_p && timercmp(&_busy_until, &now, <)) {
    output(0).push(_p);
    _p = 0;
  }

}
void 
CollisionDetection::push(int, Packet *p_in)
{
  struct timeval now;
  click_gettimeofday(&now);
  if (timercmp(&_busy_until, &now, >)) {
    /* collision */
    if (_p) {
      _p->kill();
      _p = 0;
      
    }
    _collisions++;
    p_in->kill();
    _timer.unschedule();
  } else {
    /* no collision */
    if (_p) {
      /*
       * timer hasn't gone off yet,
       * but packet is done receiving 
       */
      output(0).push(_p);
      _timer.unschedule();
    }
    _p = p_in;

    int length = p_in->length();
    timerclear(&_delay);

    _delay.tv_usec = max(100, length * 10);

    timeradd(&_delay, &now, &_busy_until);
    _timer.schedule_at(_busy_until);


  }

}

String
CollisionDetection::static_read_collisions(Element *e, void *)
{
  CollisionDetection *n = (CollisionDetection *) e;
  StringAccum sa;
  sa << n->_collisions << "\n";
  return sa.take_string();
}


String
CollisionDetection::static_read_delay(Element *e, void *)
{
  CollisionDetection *n = (CollisionDetection *) e;
  StringAccum sa;
  sa << n->_delay << "\n";
  return sa.take_string();
}

String
CollisionDetection::static_read_p(Element *e, void *)
{
  CollisionDetection *n = (CollisionDetection *) e;
  StringAccum sa;
  sa << (n->_p) << "\n";
  return sa.take_string();
}


void
CollisionDetection::add_handlers()
{
  add_read_handler("collisions", static_read_collisions, 0);
  add_read_handler("p", static_read_p, 0);
  add_read_handler("delay", static_read_delay, 0);
}

EXPORT_ELEMENT(CollisionDetection)

#include <click/bighashmap.cc>
#include <click/vector.cc>
CLICK_ENDDECLS
