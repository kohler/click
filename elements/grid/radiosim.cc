/*
 * radiosim.{cc,hh} -- simulate 802.11-like radio propagation.
 * Robert Morris
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
#include "radiosim.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "elements/standard/scheduleinfo.hh"
#include "elements/grid/filterbyrange.hh"

RadioSim::RadioSim()
{
}

RadioSim *
RadioSim::clone() const
{
  return new RadioSim;
}

int
RadioSim::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int i;

  _nodes.clear();
  for(i = 0; i < conf.size(); i++){
    Vector<String> words;
    cp_spacevec(conf[i], words);
    if(words.size() != 2)
      return errh->error("argument %d doesn't have lat and lon", i);
    int xlat, xlon;
    if(!cp_real(words[0], 5, &xlat) || !cp_real(words[1], 5, &xlon))
      return errh->error("could not parse lat or lon from arg %d", i);
    Node n;
    n._lat = ((double)xlat) / 100000.0;
    n._lon = ((double)xlon) / 100000.0;
    _nodes.push_back(n);
  }

  return 0;
}

void
RadioSim::notify_noutputs(int n)
{
  set_noutputs(n);
}

void
RadioSim::notify_ninputs(int n)
{
  set_ninputs(n);
}

int
RadioSim::initialize(ErrorHandler *errh)
{
  int n, i;
  
  n = ninputs();
  assert(n == noutputs());
  for(i = _nodes.size(); i < n; i++){
    Node no;
    no._lat = 0;
    no._lon = 0;
    _nodes.push_back(no);
  }

  ScheduleInfo::join_scheduler(this, errh);

  return 0;
}

void
RadioSim::uninitialize()
{
  unschedule();
}

void
RadioSim::run_scheduled()
{
  int in, out;

  for(in = 0; in < ninputs(); in++){
    Packet *p = input(in).pull();
    if(p){
      for(out = 0; out < noutputs(); out++){
        grid_location g1 = grid_location(_nodes[in]._lat, _nodes[in]._lon);
        grid_location g2 = grid_location(_nodes[out]._lat, _nodes[out]._lon);
        double r = FilterByRange::calc_range(g1, g2);
        if(r < 250){
          output(out).push(p->clone());
        }
      }
      p->kill();
    }
  }

  reschedule();
}

grid_location
RadioSim::get_node_loc(int i)
{
  if(i >= 0 && i < _nodes.size())
    return(grid_location(_nodes[i]._lat, _nodes[i]._lon));
  else
    return(grid_location(0, 0));
}

void
RadioSim::set_node_loc(int i, double lat, double lon)
{
  if(i >= 0 && i < _nodes.size()){
    _nodes[i]._lat = lat;
    _nodes[i]._lon = lon;
  }
}

// Expects a line that looks like
// node-index latitude longitude
static int
rs_write_handler(const String &arg, Element *element,
		  void *, ErrorHandler *errh)
{
  RadioSim *l = (RadioSim *) element;
  Vector<String> words;
  cp_spacevec(arg, words);
  int xi, xlat, xlon;
  if(words.size() != 3 ||
     !cp_integer(words[0], 10, &xi) ||
     !cp_real(words[1], 5, &xlat) ||
     !cp_real(words[2], 5, &xlon))
    return errh->error("%s: expecting node-index lat lon", l->id().cc());
  if(xi >= 0 && xi < l->nnodes()){
    double lat = ((double)xlat) / 100000.0;
    double lon = ((double)xlon) / 100000.0;
    l->set_node_loc(xi, lat, lon);
    return(0);
  } else {
    return errh->error("%s: illegal index %d", l->id().cc(), xi);
  }
}

static String
rs_read_handler(Element *f, void *)
{
  RadioSim *l = (RadioSim *) f;
  String s;
  int i, n;

  n = l->nnodes();
  for(i = 0; i < n; i++){
    grid_location loc = l->get_node_loc(i);
    const int BUFSZ = 255;
    char buf[BUFSZ];
    snprintf(buf, BUFSZ, "%d %f %f\n", i, loc.lat(), loc.lon());
    s += buf;
  }
  return s;
}

void
RadioSim::add_handlers()
{
  add_default_handlers(true);
  add_write_handler("loc", rs_write_handler, (void *) 0);
  add_read_handler("loc", rs_read_handler, (void *) 0);
}

ELEMENT_REQUIRES(FilterByRange)
EXPORT_ELEMENT(RadioSim)

#include <click/vector.cc>
template class Vector<RadioSim::Node>;
