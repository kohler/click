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
    String rest;
    unsigned long t;
    if (!cp_ulong(conf[i], &t, &rest))
      return errh->error("%s: error parsing time offset for entry %d", id().cc(), i);
    cp_eat_space(rest);
    int j;
    for (j = 0; j < rest.length() && !isspace(rest[j]); j++)
      ; // do it
    String name = rest.substring(0, j);
    rest = rest.substring(j);
    Element *el = cp_element(name, this, errh);
    if (!el) 
      return -1;
    LocationInfo *li = (LocationInfo *) el->cast("LocationInfo");
    if (!li) 
      return errh->error("element is not a LocationInfo element in entry %d", i);
    cp_eat_space(rest);
    int int_vlat, int_vlon;
    if (!cp_real(rest, 7, &int_vlat, &rest))
      return errh->error("error parsing new latitude velocity for entry %d", i);
    cp_eat_space(rest);
    if (!cp_real(rest, 7, &int_vlon, &rest))
      return errh->error("error parsing new longitude velocity for entry %d", i);
    
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

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(MovementSimulator)

#include "vector.cc"
template class Vector<MovementSimulator::node_event>;
