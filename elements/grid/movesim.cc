/*
 * movesim.{cc,hh} -- set the locations of multiple simulated grid
 * nodes using information derived from a CMU ns scenario file.
 * Douglas S. J. De Couto
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "movesim.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"



MovementSimulator::MovementSimulator() 
  : _event_timer(event_hook, (unsigned long) this)
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
    unsigned long t;
    int int_vlat, int_vlon;
    Element *el = 0;
    if (cp_va_space_parse(conf[i], this, errh,
			  cpUnsignedLong, "movement interval (ms)", &t,
			  cpElement, "LocationInfo", &el,
			  cpReal, "latitude", 7, &int_vlat,
			  cpReal, "longitude", 7, &int_vlon,
			  0) < 0)
      return -1;
    LocationInfo *li = (LocationInfo *)el->cast("LocationInfo");
    if (!li)
      return errh->error("element is not a LocationInfo in entry %d", i);
    
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
MovementSimulator::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return read_args(conf, errh);
}

int
MovementSimulator::initialize(ErrorHandler *)
{
  _event_timer.attach(this);
  // just wait a little while before we kick things off simulated
  // motions
  _event_timer.schedule_after_ms(300); 
  return 0;
}


void
MovementSimulator::event_hook(unsigned long thunk) 
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
    el->_event_timer.schedule_after_ms(el->_events->next->t - next_event->t);
  delete next_event;
}

void
MovementSimulator::add_handlers()
{
  //  add_default_handlers(true);
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

ELEMENT_REQUIRES(userlevel LocationInfo)
EXPORT_ELEMENT(MovementSimulator)

#include "vector.cc"
template class Vector<MovementSimulator::node_event>;
