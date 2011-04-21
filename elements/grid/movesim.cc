/*
 * movesim.{cc,hh} -- set the locations of multiple simulated grid
 * nodes using information derived from a CMU ns scenario file.
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
 * distribution.  */

#include <click/config.h>
#include "movesim.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

MovementSimulator::MovementSimulator()
  : _event_timer(event_hook, this)
{
  _events = new event_entry;
  _events->next = _events;
}

MovementSimulator::~MovementSimulator()
{
  event_entry *cur = _events->next;
  event_entry *next;
  while (cur != _events) {
    next = cur->next;
    delete cur;
    cur = next;
  }
  delete _events;
}

int
MovementSimulator::read_args(const Vector<String> &conf, ErrorHandler *errh)
{
  for (int i = 0; i < conf.size(); i++) {
    unsigned int t;
    int int_vlat, int_vlon;
    Element *el = 0;
    if (Args(this, errh).push_back_words(conf[i])
	.read_mp("INTERVAL", t)
	.read_mp("LOCINFO", el)
	.read_mp("LATITUDE", DecimalFixedPointArg(7), int_vlat)
	.read_mp("LONGITUDE", DecimalFixedPointArg(7), int_vlon)
	.complete() < 0)
      return -1;
    GridLocationInfo *li = (GridLocationInfo *)el->cast("GridLocationInfo");
    if (!li)
      return errh->error("element is not a GridLocationInfo in entry %d", i);

    event_entry *new_entry;
    event_entry *prev;
    bool use_prev = find_entry(t, &prev);
    if (use_prev) {
      new_entry = prev;
      assert(prev->t == t);
    }
    else {
      new_entry = new event_entry;
      new_entry->next = prev->next;
      prev->next = new_entry;
    }
    new_entry->t = t;
    node_event ne;
    ne.loc_el = li;
    ne.v_lat = ((double) int_vlat /  1.0e7);
    ne.v_lon = ((double) int_vlon /  1.0e7);
    new_entry->nodes.push_back(ne);
  }

  return 0;
}

int
MovementSimulator::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return read_args(conf, errh);
}

int
MovementSimulator::initialize(ErrorHandler *)
{
  _event_timer.initialize(this);
  // just wait a little while before we kick things off simulated
  // motions
  _event_timer.schedule_after_msec(300);
  return 0;
}


void
MovementSimulator::event_hook(Timer *, void *thunk)
{
  MovementSimulator *el = (MovementSimulator *) thunk;
  event_entry *next_event = el->_events->next;
  if (next_event == el->_events) {
    // no more events to worry about
    return;
  }
  el->_events->next = next_event->next;

  for (int i = 0; i < next_event->nodes.size(); i++)
    next_event->nodes[i].loc_el->set_new_dest(next_event->nodes[i].v_lat,
					      next_event->nodes[i].v_lon);

  if (el->_events->next != el->_events)
    el->_event_timer.schedule_after_msec(el->_events->next->t - next_event->t);
  delete next_event;
}


bool
MovementSimulator::find_entry(unsigned int t, event_entry **retval)
{
  event_entry *next = _events->next;
  event_entry *prev = _events;
  while (next != _events) {
    if (next->t == t) {
      *retval = next;
      return true;
    }
    if (next->t > t) {
      *retval = prev;
      return false;
    }
    prev = next;
    next = next->next;
  }

  *retval = _events;
  return false;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel GridLocationInfo)
EXPORT_ELEMENT(MovementSimulator)
