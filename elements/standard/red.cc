/*
 * red.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "red.hh"
#include "queue.hh"
#include <click/elemfilter.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/confparse.hh>
#include <errno.h>

RED::RED()
  : Element(1, 1)
{
}

RED *
RED::clone() const
{
  return new RED;
}

void
RED::set_C1_and_C2()
{
  if (_min_thresh >= _max_thresh) {
    _C1 = 0;
    _C2 = 1;
  } else {
    // _C1 and _C2 will not work if we keep the queue lengths scaled;
    // (_max_thresh - _min_thresh) would be so big that _C1 = _max_p/difference
    // would always be 0.
    _C1 = _max_p / ((_max_thresh - _min_thresh)>>QUEUE_SCALE);
    _C2 = (_max_p * _min_thresh) / ((_max_thresh - _min_thresh)>>QUEUE_SCALE);
  }
}

int
RED::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int min_thresh, max_thresh, max_p;
  String queues_string = String();
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "min_thresh queue length", &min_thresh,
		  cpUnsigned, "max_thresh queue length", &max_thresh,
		  cpNonnegFixed, "max_p drop probability", 16, &max_p,
		  cpOptional,
		  cpArgument, "relevant queues", &queues_string,
		  0) < 0)
    return -1;
  
  int max_allow_thresh = (0xFFFFFFFF<<QUEUE_SCALE) & ~0x80000000;
  if (min_thresh > max_allow_thresh)
    return errh->error("`min_thresh' too large (max %d)", max_allow_thresh);
  if (max_thresh > max_allow_thresh)
    return errh->error("`max_thresh' too large (max %d)", max_allow_thresh);
  if (min_thresh > max_thresh)
    return errh->error("`min_thresh' greater than `max_thresh'");
  
  if (max_p > 0x10000)
    return errh->error("`max_p' parameter must be between 0 and 1");

  // check queues_string
  if (queues_string) {
    Vector<String> eids;
    cp_spacevec(queues_string, eids);
    for (int i = 0; i < eids.size(); i++)
      if (Element *e = router()->find(this, eids[i], errh))
	_queue_elements.push_back(e);
    if (eids.size() != _queue_elements.size())
      return -1;
  }
  
  // OK: set variables
  _min_thresh = min_thresh << QUEUE_SCALE;
  _max_thresh = max_thresh << QUEUE_SCALE;
  _max_p = max_p;
  set_C1_and_C2();
  return 0;
}

int
RED::initialize(ErrorHandler *errh)
{
  if (_max_p < 0)
    return errh->error("not configured");
  
  // Find the next queues
  _queues.clear();
  _queue1 = 0;

  if (!_queue_elements.size()) {
    CastElementFilter filter("Storage");
    int ok;
    if (output_is_push(0))
      ok = router()->downstream_elements(this, 0, &filter, _queue_elements);
    else
      ok = router()->upstream_elements(this, 0, &filter, _queue_elements);
    if (ok < 0)
      return errh->error("flow-based router context failure");
    filter.filter(_queue_elements);
  }
  
  if (_queue_elements.size() == 0)
    return errh->error("no Queues downstream");
  for (int i = 0; i < _queue_elements.size(); i++)
    if (Storage *s = (Storage *)_queue_elements[i]->cast("Storage"))
      _queues.push_back(s);
    else
      errh->error("`%s' is not a Storage element", _queue_elements[i]->id().cc());
  if (_queues.size() != _queue_elements.size())
    return -1;
  else if (_queues.size() == 1)
    _queue1 = _queues[0];

  // Prepare EWMA stuff
  _size.clear();
  assert(_size.scale == QUEUE_SCALE);
  
  _drops = 0;
  _count = -1;
  return 0;
}

void
RED::take_state(Element *e, ErrorHandler *)
{
  RED *r = (RED *)e->cast("RED");
  if (!r) return;
  _size = r->_size;
}

int
RED::queue_size() const
{
  if (_queue1)
    return _queue1->size();
  else {
    int s = 0;
    for (int i = 0; i < _queues.size(); i++)
      s += _queues[i]->size();
    return s;
  }
}

bool
RED::drop()
{
  // calculate the new average queue size
  int s = queue_size();
  if (s)
    _size.update_with(s);
  else {
    // do timing stuff for when the queue was empty
    int now_j = click_jiffies();
    int j = now_j;
    for (int i = 0; i < _queues.size(); i++) {
      int ej = _queues[i]->empty_jiffies();
      if (ej - now_j > j - now_j)
	j = ej;
    }
    _size.update_zero_period(j - now_j);
  }
  
  unsigned avg = _size.average();
  if (avg <= _min_thresh)
    _count = -1;
  else if (avg > _max_thresh) {
    _count = -1;
    return true;
  } else {
    _count++;
    
    int p_b = (_C1*avg - _C2) >> QUEUE_SCALE;
    
    // note: division had Approx[]
    if (_count > 0 && p_b > 0 && _count >= _random_value / p_b) {
      _count = 0;
      _random_value = (random()>>5) & 0xFFFF;
      return true;
    } else if (_count == 0)
      _random_value = (random()>>5) & 0xFFFF;
  }
  
  return false;
}

void
RED::push(int, Packet *packet)
{
  if (drop()) {
    packet->kill();
    _drops++;
    if (_drops == 1)
      click_chatter("RED drop");
  } else
    return output(0).push(packet);
}

Packet *
RED::pull(int)
{
  while (true) {
    Packet *packet = input(0).pull();
    if (!packet || !drop())
      return packet;
    packet->kill();
    _drops++;
    click_chatter("RED drop");
  }
}


// HANDLERS

static String
red_read_drops(Element *f, void *)
{
  RED *r = (RED *)f;
  return String(r->drops()) + "\n";
}

String
RED::read_parameter(Element *f, void *vparam)
{
  RED *red = (RED *)f;
  switch ((int)vparam) {
   case 0:			// min_thresh
    return String(red->_min_thresh>>QUEUE_SCALE) + "\n";
   case 1:			// max_thresh
    return String(red->_max_thresh>>QUEUE_SCALE) + "\n";
   case 2:			// max_p
    return cp_unparse_real(red->_max_p, 16) + "\n";
   default:
    return "";
  }
}

String
RED::read_stats(Element *f, void *)
{
  RED *r = (RED *)f;
  const DirectEWMA &ewma = r->average_queue_size();
  return
    String(r->queue_size()) + " current queue\n" +
    cp_unparse_real(ewma.average(), ewma.scale) + " avg queue\n" +
    String(r->drops()) + " drops\n"
#if CLICK_STATS >= 1
    + String(r->output(0).packet_count()) + " packets\n"
#endif
    ;
}

String
RED::read_queues(Element *f, void *)
{
  RED *r = (RED *)f;
  String s;
  for (int i = 0; i < r->_queue_elements.size(); i++)
    s += r->_queue_elements[i]->id() + "\n";
  return s;
}

void
RED::add_handlers()
{
  add_read_handler("drops", red_read_drops, 0);
  add_read_handler("stats", read_stats, 0);
  add_read_handler("queues", read_queues, 0);
  add_read_handler("min_thresh", read_parameter, (void *)0);
  add_write_handler("min_thresh", reconfigure_write_handler, (void *)0);
  add_read_handler("max_thresh", read_parameter, (void *)1);
  add_write_handler("max_thresh", reconfigure_write_handler, (void *)1);
  add_read_handler("max_p", read_parameter, (void *)2);
  add_write_handler("max_p", reconfigure_write_handler, (void *)2);
}

ELEMENT_REQUIRES(Storage)
EXPORT_ELEMENT(RED)
