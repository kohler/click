/*
 * regionpep.{cc,hh} -- Region-based Grid Position Estimation Protocol
 * Douglas S. J. De Couto.  from pep.{cc,hh} by Robert Morris.
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
#include "regionpep.hh"
#include "confparse.hh"
#include "error.hh"
#include "grid.hh"

EstimateRouterRegion::EstimateRouterRegion()
  : _timer(this)
{
  add_input();
  add_output();
  _fixed = false;
  _seq = 1;
  _debug = false;
}

EstimateRouterRegion::~EstimateRouterRegion()
{
}

EstimateRouterRegion *
EstimateRouterRegion::clone() const
{
  return new EstimateRouterRegion;
}

void *
EstimateRouterRegion::cast(const char *name)
{
  if(strcmp(name, "LocationInfo") == 0)
    return(this);
  return(LocationInfo::cast(name));
}

int
EstimateRouterRegion::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int lat_int = 0, lon_int = 0;
  bool fixed = false;
  int res = cp_va_parse(conf, this, errh,
			cpIPAddress, "source IP address", &_my_ip,
                        cpOptional,
                        cpBool, "fixed?", &fixed,
			cpReal, "latitude (decimal degrees)", 5, &lat_int,
			cpReal, "longitude (decimal degrees)", 5, &lon_int,
			0);
  if(res < 0)
    return(res);

  if(fixed){
    float lat = ((float) lat_int) / 100000.0f;
    float lon = ((float) lon_int) / 100000.0f; 
    if (lat > 90 || lat < -90)
      return errh->error("%s: latitude must be between +/- 90 degrees",
                         id().cc());
    if (lon > 180 || lon < -180)
      return errh->error("%s: longitude must be between +/- 180 degrees",
                         id().cc());
    _lat = lat;
    _lon = lon;
    _fixed = true;
  }

  return(0);
}

int
EstimateRouterRegion::initialize(ErrorHandler *)
{
  _timer.attach(this);
  _timer.schedule_after_ms(pep_update * 1000);
  return 0;
}

void
EstimateRouterRegion::run_scheduled()
{
  purge_old();
  output(0).push(make_PEP());
  _timer.schedule_after_ms(random() % (pep_update * 1000 * 2));
}

void
EstimateRouterRegion::purge_old()
{
  struct timeval tv;

  click_gettimeofday(&tv);

  int i = 0;
  int j;
  for(j = 0; j < _entries.size(); j++){
    if(tv.tv_sec - _entries[j]._when.tv_sec <= pep_purge){
      _entries[i++] = _entries[j];
    } else {
      if(_debug)
        click_chatter("EstimateRouterRegion %s %s: purging old entry for %s (%d %d %d)",
                      id().cc(),
                      _my_ip.s().cc(),
                      IPAddress(_entries[j]._fix.fix_id).s().cc(),
                      (int) _entries[j]._when.tv_sec,
                      (int) tv.tv_sec,
                      pep_purge);
    }
  }

  static Entry dud;
  _entries.resize(i, dud);
}

void
EstimateRouterRegion::sort_entries()
{
  int n = _entries.size();
  int i, j;

  for(i = 0; i < n; i++){
    int h = _entries[i]._fix.fix_hops;
    for(j = i + 1; j < n; j++){
      if(_entries[j]._fix.fix_hops < h){
        Entry tmp = _entries[i];
        _entries[i] = _entries[j];
        _entries[j] = tmp;
      }
    }
  }
}

// Are we allowed to send a particular update?
bool
EstimateRouterRegion::sendable(Entry e)
{
  struct timeval tv;

  click_gettimeofday(&tv);
  if(e._when.tv_sec + pep_stale > tv.tv_sec &&
     e._fix.fix_hops < pep_max_hops){
    return(true);
  }

  return(false);
}

void
EstimateRouterRegion::externalize(pep_rgn_fix *fp)
{
  fp->fix_seq = htonl(fp->fix_seq);
  fp->fix_hops = htonl(fp->fix_hops);
}

void
EstimateRouterRegion::internalize(pep_rgn_fix *fp)
{
  fp->fix_seq = ntohl(fp->fix_seq);
  fp->fix_hops = ntohl(fp->fix_hops);
}

Packet *
EstimateRouterRegion::make_PEP()
{
  WritablePacket *p = Packet::make(sizeof(pep_rgn_proto));
  memset(p->data(), 0, p->length());

  pep_rgn_proto *pp = (pep_rgn_proto *) p->data();
  pp->id = _my_ip.addr();
  int nf = 0;

  if(_fixed){
    pep_rgn_fix *f = pp->fixes + nf;
    nf++;
    f->fix_id = _my_ip.addr();
    f->fix_seq = htonl(_seq++);
    f->fix_loc = grid_location(_lat, _lon);
    f->fix_dim = grid_location(0, 0);
    f->fix_hops = htonl(0);
  }
  else if (_entries.size() > 0) {
    // always include the best estimate of our region
    pep_rgn_fix *f = pp->fixes + nf;
    nf++;
    f->fix_id = _my_ip.addr();
    f->fix_seq = htonl(_seq++);
    RectRegion rgn = build_region();
    f->fix_loc = grid_location(rgn.y(), rgn.x());
    f->fix_dim = grid_location(rgn.h(), rgn.w());
    f->fix_hops = htonl(0);
  }

  sort_entries();

  int i;
  for(i = 0; i < _entries.size() && nf < pep_proto_fixes; i++){
    if(sendable(_entries[i])){
      pp->fixes[nf] = _entries[i]._fix;
      externalize(&(pp->fixes[nf]));
      nf++;
    }
  }

  pp->n_fixes = htonl(nf);

  return p;
}

int
EstimateRouterRegion::findEntry(unsigned id, bool create)
{
  int i;

  for(i = 0; i < _entries.size(); i++)
    if(_entries[i]._fix.fix_id == id)
      return(i);

  if (create) {
    if (_debug)
      click_chatter("EstimateRouterRegion %s %s: new entry for %s",
                    this->id().cc(),
                    _my_ip.s().cc(),
                    IPAddress(id).s().cc());
    i = _entries.size();
    static Entry e;
    e._fix.fix_id = id;
    e._fix.fix_hops = -1;
    e._fix.fix_seq = -1;
    _entries.push_back(e);
    assert(_entries.size() == i+1 && _entries[i]._fix.fix_id == id);
    return(i);
  } else {
    return(-1);
  }
}

Packet *
EstimateRouterRegion::simple_action(Packet *p)
{
  int nf;
  pep_rgn_proto *pp;
  struct timeval tv;

  click_gettimeofday(&tv);

  if(p->length() != sizeof(pep_rgn_proto)) {
    click_chatter("EstimateRouterRegion: bad size packet (%d bytes)", p->length());
    goto out;
  }

  pp = (pep_rgn_proto *) p->data();
  nf = ntohl(pp->n_fixes);
  if(nf < 0 || (const u_char*)&pp->fixes[nf] > p->data()+p->length()){
    click_chatter("EstimateRouterRegion: bad n_fixes %d", nf);
    goto out;
  }

  int i;
  for(i = 0; i < nf && i < pep_proto_fixes; i++){
    pep_rgn_fix f = pp->fixes[i];
    internalize(&f);
    if(f.fix_id == _my_ip.addr())
      continue;
    int j = findEntry(f.fix_id, true);
    if(j < 0)
      continue;

    int os = _entries[j]._fix.fix_seq;
    int oh = _entries[j]._fix.fix_hops;
    if(f.fix_seq > os ||
       (f.fix_seq == os && (f.fix_hops+1) < oh)){
      _entries[j]._fix = f;
      _entries[j]._fix.fix_hops++;
      _entries[j]._when = tv;
      if(_debug && f.fix_hops != oh)
        click_chatter("EstimateRouterRegion %s %s: updating %s, seq %d -> %d, hops %d -> %d, my pos %s",
                      id().cc(),
                      _my_ip.s().cc(),
                      IPAddress(f.fix_id).s().cc(),
                      os,
                      f.fix_seq,
                      oh,
                      f.fix_hops,
                      get_current_location().s().cc());
    }
  }

 out:
  p->kill();
  return(0);
}

double 
EstimateRouterRegion::radio_range(grid_location) // XXX degrees lat covered by range
{ return 0.002; }

RectRegion
EstimateRouterRegion::build_region()
{
  pep_rgn_fix f = _entries[0]._fix;
  RectRegion rgn(f.fix_loc.lon(), f.fix_loc.lat(), f.fix_dim.lon(), f.fix_dim.lat());
  rgn = rgn.expand(f.fix_hops * radio_range(f.fix_loc));

  int num_skips = 0;
  for (int i = 1; i < _entries.size(); i++) {
    f = _entries[i]._fix;
    RectRegion rgn2(f.fix_loc.lon(), f.fix_loc.lat(), f.fix_dim.lon(), f.fix_dim.lat());
    rgn2 = rgn2.expand(f.fix_hops * radio_range(f.fix_loc));

    RectRegion new_rgn = rgn2.intersect(rgn);
    if (new_rgn.empty()) {
      click_chatter("EstimateRouterRegion %s: skipping region which would result in empty intersection (%d)", id().cc(), ++num_skips);
      continue;
    }
    rgn = new_rgn;
  }

  return rgn;
}

grid_location
EstimateRouterRegion::get_current_location()
{
  if(_fixed)
    return(grid_location(_lat, _lon));

  if(_entries.size() < 1)
    return(grid_location(0.0, 0.0));

  RectRegion rgn = build_region();
  return grid_location(rgn.center_y(), rgn.center_x());
}

static String
pep_read_handler(Element *f, void *)
{
  EstimateRouterRegion *l = (EstimateRouterRegion *) f;
  return(l->s());
}

String
EstimateRouterRegion::s()
{
  String s;
  int i, n;
  struct timeval now;

  click_gettimeofday(&now);

  if(_fixed){
    s = _my_ip.s() + " " +
      grid_location(_lat, _lon).s() + "\n";
  } else {
    s = _my_ip.s() + "\n";
  }

  s += get_current_location().s() + "\n";

  n = _entries.size();
  for(i = 0; i < n; i++){
    pep_rgn_fix f = _entries[i]._fix;
    char buf[512];
    snprintf(buf, sizeof(buf), "%s seq=%d %s hops=%d age=%d\n",
             IPAddress(f.fix_id).s().cc(),
             f.fix_seq,
             f.fix_loc.s().cc(),
             f.fix_hops,
             (int)(now.tv_sec - _entries[i]._when.tv_sec));
    s += buf;
  }
  return s;
}

void
EstimateRouterRegion::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("status", pep_read_handler, (void *) 0);
}

ELEMENT_REQUIRES(LocationInfo)
EXPORT_ELEMENT(EstimateRouterRegion)

#include "vector.cc"
template class Vector<EstimateRouterRegion::Entry>;
