/*
 * pep.{cc,hh} -- Grid Position Estimation Protocol
 * Robert Morris
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
#include "pep.hh"
#include "confparse.hh"
#include "error.hh"
#include "grid.hh"

PEP::PEP()
  : _timer(this)
{
  add_input();
  add_output();
  _fixed = 0;
  _seq = 1;
  _debug = true;
}

PEP::~PEP()
{
}

PEP *
PEP::clone() const
{
  return new PEP;
}

void *
PEP::cast(const char *name)
{
  if(strcmp(name, "LocationInfo") == 0)
    return(this);
  return(LocationInfo::cast(name));
}

int
PEP::configure(const Vector<String> &conf, ErrorHandler *errh)
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
PEP::initialize(ErrorHandler *)
{
  _timer.attach(this);
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
        click_chatter("PEP %s: purging old entry for %s (%d %d %d)",
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
      click_chatter("PEP %s: new entry for %s",
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
  if(nf < 0 || (const u_char*)&pp->fixes[nf + 1] > p->data()+p->length()){
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
        click_chatter("PEP %s: updating %s, seq %d -> %d, hops %d -> %d, my pos %s",
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
PEP::get_current_location()
{
  double lat = 0, lon = 0;
  double weight = 0;
  int i;
  struct timeval now;

  if(_fixed)
    return(grid_location(_lat, _lon));

  if(_entries.size() < 1)
    return(grid_location(0.0, 0.0));

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


EXPORT_ELEMENT(PEP)

#include "vector.cc"
template class Vector<PEP::Entry>;
