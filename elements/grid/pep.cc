/*
 * pep.{cc,hh} -- Grid Position Estimation Protocol
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
#include "pep.hh"
#include "amoeba.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include "grid.hh"
#include <math.h>

PEP::PEP()
  : _timer(this)
{
  // no MOD_INC_USE_COUNT; rely on GridLocationInfo
  add_input();
  add_output();
  _fixed = 0;
  _seq = 1;
  _debug = false;
}

PEP::~PEP()
{
  // no MOD_DEC_USE_COUNT; rely on GridLocationInfo
}

PEP *
PEP::clone() const
{
  return new PEP;
}

void *
PEP::cast(const char *name)
{
  if(strcmp(name, "GridLocationInfo") == 0)
    return(this);
  return(GridLocationInfo::cast(name));
}

int
PEP::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int lat_int = 0, lon_int = 0;
  bool fixed = false;
  int res = cp_va_parse(conf, this, errh,
			cpIPAddress, "source IP address", &_my_ip,
                        cpOptional,
                        cpBool, "fixed?", &fixed,
			cpReal10, "latitude (decimal degrees)", 5, &lat_int,
			cpReal10, "longitude (decimal degrees)", 5, &lon_int,
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
PEP::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_ms(pep_update * 1000);
  return 0;
}

void
PEP::run_scheduled()
{
  purge_old();
  output(0).push(make_PEP());
  _timer.schedule_after_ms(random() % (pep_update * 1000 * 2));
}

void
PEP::purge_old()
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
        click_chatter("PEP %s %s: purging old entry for %s (%d %d %d)",
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
PEP::sort_entries()
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
PEP::sendable(Entry e)
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
PEP::externalize(pep_fix *fp)
{
  fp->fix_seq = htonl(fp->fix_seq);
  fp->fix_hops = htonl(fp->fix_hops);
}

void
PEP::internalize(pep_fix *fp)
{
  fp->fix_seq = ntohl(fp->fix_seq);
  fp->fix_hops = ntohl(fp->fix_hops);
}

Packet *
PEP::make_PEP()
{
  WritablePacket *p = Packet::make(sizeof(pep_proto));
  memset(p->data(), 0, p->length());

  pep_proto *pp = (pep_proto *) p->data();
  pp->id = _my_ip.addr();
  int nf = 0;

  if(_fixed){
    pep_fix *f = pp->fixes + nf;
    nf++;
    f->fix_id = _my_ip.addr();
    f->fix_seq = htonl(_seq++);
    f->fix_loc = grid_location(_lat, _lon);
    f->fix_hops = htonl(0);
  }

  sort_entries();

  int i;
  for(i = 0; i < _entries.size() && nf < pep_proto_fixes; i++){
    if(sendable(_entries[i])){
      pp->fixes[nf] = _entries[i]._fix;
      pp->fixes[nf].fix_hops += 1;
      externalize(&(pp->fixes[nf]));
      nf++;
    }
  }

  pp->n_fixes = htonl(nf);

  return p;
}

int
PEP::findEntry(unsigned id, bool create)
{
  int i;

  for(i = 0; i < _entries.size(); i++)
    if(_entries[i]._fix.fix_id == id)
      return(i);

  if(create){
    if(_debug)
      click_chatter("PEP %s %s: new entry for %s",
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
PEP::simple_action(Packet *p)
{
  int nf;
  pep_proto *pp;
  struct timeval tv;

  click_gettimeofday(&tv);

  if(p->length() != sizeof(pep_proto)){
    click_chatter("PEP: bad size packet (%d bytes)", p->length());
    goto out;
  }

  pp = (pep_proto *) p->data();
  nf = ntohl(pp->n_fixes);
  if(nf < 0 || (const u_char*)&pp->fixes[nf] > p->data()+p->length()){
    click_chatter("PEP: bad n_fixes %d", nf);
    goto out;
  }

  int i;
  for(i = 0; i < nf && i < pep_proto_fixes; i++){
    pep_fix f = pp->fixes[i];
    internalize(&f);
    if(f.fix_id == _my_ip.addr())
      continue;
    int j = findEntry(f.fix_id, true);
    if(j < 0)
      continue;

    int os = _entries[j]._fix.fix_seq;
    int oh = _entries[j]._fix.fix_hops;
    if(f.fix_seq > os ||
       (f.fix_seq == os && f.fix_hops < oh)){
      _entries[j]._fix = f;
      _entries[j]._when = tv;
      if(_debug && f.fix_hops != oh)
        click_chatter("PEP %s %s: updating %s, seq %d -> %d, hops %d -> %d, my pos %s",
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

// Actually guess where we are.
grid_location
PEP::algorithm1()
{
  double lat = 0, lon = 0;
  double weight = 0;
  int i;
  struct timeval now;

  click_gettimeofday(&now);

  for(i = 0; i < _entries.size(); i++){
    if(now.tv_sec - _entries[i]._when.tv_sec < pep_stale){
      double w = 1.0 / (_entries[i]._fix.fix_hops + 1);
      lat += w * _entries[i]._fix.fix_loc.lat();
      lon += w * _entries[i]._fix.fix_loc.lon();
      weight += w;
    }
  }
  
  return(grid_location(lat / weight, lon / weight));
}

class PEPAmoeba : public Amoeba {
public:
  PEPAmoeba () : Amoeba(2) { }
  double fn(double a[]);
  int _n;
  double _x[20];
  double _y[20];
  double _d[20];
};

double
PEPAmoeba::fn(double a[])
{
  double x = a[0];
  double y = a[1];
  double d = 0;
  int i;

  for(i = 0; i < _n; i++){
    double dx = x - _x[i];
    double dy = y - _y[i];
    double dd = _d[i] - sqrt(dx*dx + dy*dy);
    dd = dd * dd;
    d += dd;
  }

  return(d);
}

grid_location
PEP::algorithm2()
{
  PEPAmoeba a;
  int i;
  struct timeval now;
  
  click_gettimeofday(&now);
  a._n = 0;

  for(i = 0; i < _entries.size() && a._n < 5; i++){
    if(now.tv_sec - _entries[i]._when.tv_sec < pep_stale){
      a._x[a._n] = _entries[i]._fix.fix_loc.lat();
      a._y[a._n] = _entries[i]._fix.fix_loc.lat();
      a._d[a._n] = _entries[i]._fix.fix_hops * 0.002; // XXX 250 meters?  should be fix_hops + 1??
      a._n += 1;
    }
  }

  double pts[2];
  a.minimize(pts);
  
  return(grid_location(pts[0], pts[1]));
}

grid_location
PEP::get_current_location()
{
  if(_fixed)
    return(grid_location(_lat, _lon));

  if(_entries.size() < 1)
    return(grid_location(0.0, 0.0));

  return(algorithm1());
}

static String
pep_read_handler(Element *f, void *)
{
  PEP *l = (PEP *) f;
  return(l->s());
}

String
PEP::s()
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
    pep_fix f = _entries[i]._fix;
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
PEP::add_handlers()
{
  add_default_handlers(true);
  add_read_handler("status", pep_read_handler, (void *) 0);
}

ELEMENT_REQUIRES(GridLocationInfo Amoeba)
EXPORT_ELEMENT(PEP)

#include <click/vector.cc>
template class Vector<PEP::Entry>;
