/*
 * red.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
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
#include "red.hh"
#include "queue.hh"
#include "elemfilter.hh"
#include "error.hh"
#include "router.hh"
#include "confparse.hh"
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
RED::configure(const String &conf, ErrorHandler *errh)
{
  int min_thresh, max_thresh, max_p;
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "min_thresh queue length", &min_thresh,
		  cpUnsigned, "max_thresh queue length", &max_thresh,
		  cpNonnegReal2, "max_p drop probability", 16, &max_p,
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
  
  IsaElementFilter filter("Storage");
  int ok;
  if (output_is_push(0))
    ok = router()->downstream_elements(this, 0, &filter, _queues);
  else
    ok = router()->upstream_elements(this, 0, &filter, _queues);
  if (ok < 0)
    return errh->error("downstream_elements failure");
  filter.filter(_queues);
  
  if (_queues.size() == 0)
    return errh->error("no Queues downstream");
  else if (_queues.size() == 1)
    _queue1 = (Storage *)_queues[0];

  // Prepare EWMA stuff
  _size.initialize();
  if (_size.scale() != QUEUE_SCALE)
    return errh->error("fix Click source: EWMA scale not %d", QUEUE_SCALE);
  
  _drops = 0;
  _count = -1;
  return 0;
}

int
RED::queue_size() const
{
  if (_queue1)
    return _queue1->size();
  else {
    int s = 0;
    for (int i = 0; i < _queues.size(); i++)
      s += ((Storage *)_queues[i])->size();
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
      int ej = ((Storage *)_queues[i])->empty_jiffies();
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
  const EWMA &ewma = r->average_queue_size();
  return
    String(r->queue_size()) + " current queue\n" +
    cp_unparse_real(ewma.average(), ewma.scale()) + " avg queue\n" +
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
  if (r->_queue1)
    return r->_queue1->id() + "\n";
  else {
    String s;
    for (int i = 0; i < r->_queues.size(); i++)
      s += r->_queues[i]->id() + "\n";
    return s;
  }
}

void
RED::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("drops", red_read_drops, 0);
  fcr->add_read("stats", read_stats, 0);
  fcr->add_read("queues", read_queues, 0);
  fcr->add_read_write("min_thresh", read_parameter, (void *)0,
		      reconfigure_write_handler, (void *)0);
  fcr->add_read_write("max_thresh", read_parameter, (void *)1,
		      reconfigure_write_handler, (void *)1);
  fcr->add_read_write("max_p", read_parameter, (void *)2,
		      reconfigure_write_handler, (void *)2);
}

ELEMENT_REQUIRES(Storage)
EXPORT_ELEMENT(RED)
