/*
 * red.{cc,hh} -- element implements Random Early Detection dropping policy
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 ACIRI
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
#include "red.hh"
#include "queue.hh"
#include <click/elemfilter.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#define RED_DEBUG 0

RED::RED()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

RED::~RED()
{
  MOD_DEC_USE_COUNT;
}

RED *
RED::clone() const
{
  return new RED;
}

void
RED::notify_noutputs(int n)
{
  set_noutputs(n <= 1 ? 1 : 2);
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
RED::check_thresh_and_p(unsigned min_thresh, unsigned max_thresh,
			unsigned max_p, ErrorHandler *errh) const
{
  unsigned max_allow_thresh = (0xFFFFFFFF<<QUEUE_SCALE);
  
  if (min_thresh > max_allow_thresh)
    return errh->error("`min_thresh' too large (max %d)", max_allow_thresh);
  if (max_thresh > max_allow_thresh)
    return errh->error("`max_thresh' too large (max %d)", max_allow_thresh);
  if (min_thresh > max_thresh)
    return errh->error("`min_thresh' greater than `max_thresh'");
  if (max_p > 0x10000)
    return errh->error("`max_p' parameter must be between 0 and 1");

  return 0;
}

int
RED::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned min_thresh, max_thresh, max_p;
  String queues_string = String();
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "min_thresh queue length", &min_thresh,
		  cpUnsigned, "max_thresh queue length", &max_thresh,
		  cpUnsignedReal2, "max_p drop probability", 16, &max_p,
		  cpOptional,
		  cpArgument, "relevant queues", &queues_string,
		  cpKeywords,
		  "MIN_THRESH", cpUnsigned, "min_thresh queue length", &min_thresh,
		  "MAX_THRESH", cpUnsigned, "max_thresh queue length", &max_thresh,
		  "MAX_P", cpUnsignedReal2, "max_p drop probability", 16, &max_p,
		  "QUEUES", cpArgument, "relevant queues", &queues_string,
		  0) < 0)
    return -1;

  if (check_thresh_and_p(min_thresh, max_thresh, max_p, errh) < 0)
    return -1;

  // check queues_string
  if (queues_string) {
    Vector<String> eids;
    cp_spacevec(queues_string, eids);
    _queue_elements.clear();
    for (int i = 0; i < eids.size(); i++)
      if (Element *e = router()->find(eids[i], this, errh))
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
RED::live_reconfigure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned min_thresh, max_thresh, max_p;
  String queues_string = String();
  if (cp_va_parse(conf, this, errh,
		  cpUnsigned, "min_thresh queue length", &min_thresh,
		  cpUnsigned, "max_thresh queue length", &max_thresh,
		  cpUnsignedReal2, "max_p drop probability", 16, &max_p,
		  cpOptional,
		  cpArgument, "relevant queues", &queues_string,
		  cpKeywords,
		  "MIN_THRESH", cpUnsigned, "min_thresh queue length", &min_thresh,
		  "MAX_THRESH", cpUnsigned, "max_thresh queue length", &max_thresh,
		  "MAX_P", cpUnsignedReal2, "max_p drop probability", 16, &max_p,
		  "QUEUES", cpArgument, "relevant queues", &queues_string,
		  0) < 0)
    return -1;

  if (check_thresh_and_p(min_thresh, max_thresh, max_p, errh) < 0)
    return -1;
  
  if (queues_string)
    errh->warning("QUEUES argument ignored");
  
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
  _last_jiffies = 0;
  return 0;
}

void
RED::take_state(Element *e, ErrorHandler *)
{
  RED *r = (RED *)e->cast("RED");
  if (!r) return;
  _size = r->_size;
}

void
RED::configuration(Vector<String> &conf, bool *) const
{
  conf.push_back(String(_min_thresh >> QUEUE_SCALE));
  conf.push_back(String(_max_thresh >> QUEUE_SCALE));
  conf.push_back(cp_unparse_real2(_max_p, 16));

  StringAccum sa;
  sa << "QUEUES";
  for (int i = 0; i < _queue_elements.size(); i++)
    sa << ' ' << _queue_elements[i]->id();
  conf.push_back(sa.take_string());
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
RED::should_drop()
{
  // calculate the new average queue size.
  // Do some rigamarole to handle empty periods, but don't work too hard.
  // (Therefore it contains errors. XXX)
  int s = queue_size();
  if (s) {
    _size.update_with(s);
    _last_jiffies = 0;
  } else {
    // do timing stuff for when the queue was empty
#if CLICK_HZ < 50
    int j = click_jiffies();
#else
    int j = click_jiffies() / (CLICK_HZ / 50);
#endif
    _size.update_zero_period(_last_jiffies ? j - _last_jiffies : 1);
    _last_jiffies = j;
  }
  
  unsigned avg = _size.average();
  if (avg <= _min_thresh)
    _count = -1;
  else if (avg > _max_thresh) {
    _count = -1;
#if RED_DEBUG
    click_chatter("%s: drop, over max_thresh", declaration().cc());
#endif
    return true;
  } else {
    _count++;
    
    int p_b = (_C1*avg - _C2) >> QUEUE_SCALE;
    
    // note: division had Approx[]
    if (_count > 0 && p_b > 0 && _count > _random_value / p_b) {
#if RED_DEBUG
      click_chatter("%s: drop, random drop (%d, %d, %d, %d)", declaration().cc(), _count, p_b, _random_value, _random_value/p_b);
#endif
      _count = 0;
      _random_value = (random()>>5) & 0xFFFF;
      return true;
    } else if (_count == 0)
      _random_value = (random()>>5) & 0xFFFF;
  }
  
#if RED_DEBUG
  click_chatter("%s: no drop", declaration().cc());
#endif
  return false;
}

inline void
RED::handle_drop(Packet *p)
{
  if (noutputs() == 1)
    p->kill();
  else
    output(1).push(p);
  _drops++;
}

void
RED::push(int, Packet *p)
{
  if (should_drop())
    handle_drop(p);
  else
    output(0).push(p);
}

Packet *
RED::pull(int)
{
  while (true) {
    Packet *p = input(0).pull();
    if (!p)
      return 0;
    else if (!should_drop())
      return p;
    handle_drop(p);
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
    return cp_unparse_real2(red->_max_p, 16) + "\n";
   case 3:			// avg_queue_size
    return red->_size.unparse() + "\n";
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
    cp_unparse_real2(ewma.average(), ewma.scale) + " avg queue\n" +
    String(r->drops()) + " drops\n"
#if CLICK_STATS >= 1
    + String(r->output(0).npackets()) + " packets\n"
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
  add_write_handler("min_thresh", reconfigure_positional_handler_2, (void *)0);
  add_read_handler("max_thresh", read_parameter, (void *)1);
  add_write_handler("max_thresh", reconfigure_positional_handler_2, (void *)1);
  add_read_handler("max_p", read_parameter, (void *)2);
  add_write_handler("max_p", reconfigure_positional_handler_2, (void *)2);
  add_read_handler("avg_queue_size", read_parameter, (void *)3);
}

ELEMENT_REQUIRES(Storage)
EXPORT_ELEMENT(RED)
