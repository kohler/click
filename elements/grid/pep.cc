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
  : Element(1, 1), _timer(this)
{
  _fixed = 0;
  _period = 1000;

  int i;
  for(i = 0; i < PEPMaxEntries; i++)
    _entries[i]._valid = false;
}

PEP::~PEP()
{
}

PEP *
PEP::clone() const
{
  return new PEP;
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
  _timer.schedule_after_ms(_period);
  return 0;
}

void
PEP::run_scheduled()
{
  output(0).push(make_PEP());
  _timer.schedule_after_ms(random() % (_period * 2));
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
    f->fix_loc = grid_location(_lat, _lon);
    f->fix_d = htonl(0);
  }

  int i;
  for(i = 0; i < PEPMaxEntries && nf < pep_proto_fixes; i++){
    Entry *e = _entries + i;
    if(e->_valid){
      pep_fix *f = pp->fixes + nf;
      nf++;
      f->fix_id = e->_id;
      f->fix_loc = e->_loc;
      f->fix_d = htonl(e->_d + 250);
    }
  }

  pp->n_fixes = htonl(nf);

  return p;
}

PEP::Entry *
PEP::findEntry(unsigned id, int allocate)
{
  int i;
  int unused = -1;
  
  for(i = 0; i < PEPMaxEntries; i++){
    if(_entries[i]._valid && _entries[i]._id == id){
      return(_entries + i);
    }
    if(_entries[i]._valid == 0)
      unused = i;
  }

  if(allocate && unused != -1){
    Entry *e = _entries + unused;
    e->_valid = true;
    e->_id = id;
    return(e);
  }

  return(0);
}

Packet *
PEP::simple_action(Packet *p)
{
  int nf;
  pep_proto *pp;

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

  pep_fix *f;
  for(f = pp->fixes; f < pp->fixes + nf; f++){
    if(f->fix_id != _my_ip.addr()){
      Entry *e = findEntry(f->fix_id, 1);
      if(e){
        e->_loc = f->fix_loc;
        e->_d = ntohl(f->fix_d);
      }
    }
  }

 out:
  p->kill();
  return(0);
}

EXPORT_ELEMENT(PEP)
