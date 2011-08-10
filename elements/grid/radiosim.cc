/*
 * radiosim.{cc,hh} -- simulate 802.11-like radio propagation.
 * Robert Morris
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
 */

#include <click/config.h>
#include "radiosim.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <math.h>
#include "elements/grid/filterbyrange.hh"
CLICK_DECLS

RadioSim::RadioSim()
  : _task(this), _use_xy(false)
{
}

RadioSim::~RadioSim()
{
}

int
RadioSim::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int i;

  _nodes.clear();
  for(i = 0; i < conf.size(); i++){
    // check for keywords
    String kw;
    String rest;
    if (cp_keyword(conf[i], &kw, &rest)) {
      if (kw == "USE_XY") {
	  bool res = BoolArg().parse(rest, _use_xy);
	  if (!res)
	      return errh->error("unable to parse boolean arg to USE_XY keyword");
	  continue;
      }
    }

    // otherwise it's coordinates
    Vector<String> words;
    cp_spacevec(conf[i], words);
    if(words.size() != 2)
      return errh->error("argument %d doesn't have lat and lon", i);
    int xlat, xlon;
    if(!cp_real10(words[0], 5, &xlat) || !cp_real10(words[1], 5, &xlon))
      return errh->error("could not parse lat or lon from arg %d", i);
    Node n;
    n._lat = ((double)xlat) / 100000.0;
    n._lon = ((double)xlon) / 100000.0;
    _nodes.push_back(n);
  }

  return 0;
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

  ScheduleInfo::join_scheduler(this, &_task, errh);

  return 0;
}

bool
RadioSim::run_task(Task *)
{
  int in, out;

  for(in = 0; in < ninputs(); in++){
    Packet *p = input(in).pull();
    if(p){
      for(out = 0; out < noutputs(); out++){
	double r;
	if (_use_xy) {
	  double dx = _nodes[in]._lat - _nodes[out]._lat;
	  double dy = _nodes[in]._lon - _nodes[out]._lon;
	  r = sqrt(dx*dx + dy*dy);
	}
	else {
	  grid_location g1 = grid_location(_nodes[in]._lat, _nodes[in]._lon);
	  grid_location g2 = grid_location(_nodes[out]._lat, _nodes[out]._lon);
	  r = grid_location::calc_range(g1, g2);
	}
        if (r < 250){
          output(out).push(p->clone());
        }
      }
      p->kill();
    }
  }

  _task.fast_reschedule();
  return true;
}

RadioSim::Node
RadioSim::get_node_loc(int i)
{
  if(i >= 0 && i < _nodes.size())
    return _nodes[i];
  else
    return Node(0, 0);
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
int
RadioSim::rs_write_handler(const String &arg, Element *element,
			   void *, ErrorHandler *errh)
{
  RadioSim *l = (RadioSim *) element;
  Vector<String> words;
  cp_spacevec(arg, words);
  int xi, xlat, xlon;
  if(words.size() != 3 ||
     !IntArg(10).parse(words[0], xi) ||
     !cp_real10(words[1], 5, &xlat) ||
     !cp_real10(words[2], 5, &xlon))
    return errh->error("%s: expecting node-index lat lon", l->name().c_str());
  if(xi >= 0 && xi < l->nnodes()){
    double lat = ((double)xlat) / 100000.0;
    double lon = ((double)xlon) / 100000.0;
    l->set_node_loc(xi, lat, lon);
    return(0);
  } else {
    return errh->error("%s: illegal index %d", l->name().c_str(), xi);
  }
}

String
RadioSim::rs_read_handler(Element *f, void *)
{
  RadioSim *l = (RadioSim *) f;
  String s;
  int i, n;

  n = l->nnodes();
  for(i = 0; i < n; i++){
    Node n = l->get_node_loc(i);
    const int BUFSZ = 255;
    char buf[BUFSZ];
    snprintf(buf, BUFSZ, "%d %f %f\n", i, n._lat, n._lon);
    s += buf;
  }
  return s;
}

void
RadioSim::add_handlers()
{
  add_write_handler("loc", rs_write_handler, 0);
  add_read_handler("loc", rs_read_handler, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FilterByRange)
EXPORT_ELEMENT(RadioSim)
