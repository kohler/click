/*
 * locfromfile.{cc,hh} -- play a trace of locations.
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
#include "locfromfile.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"

LocFromFile::LocFromFile()
{
  _next = 0;
  _move = 1;
}

LocFromFile::~LocFromFile()
{
}

void *
LocFromFile::cast(const char *name)
{
  if(strcmp(name, "LocationInfo") == 0)
    return(this);
  return(LocationInfo::cast(name));
}

int
LocFromFile::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String filename;
  int res = cp_va_parse(conf, this, errh,
                        cpString, "filename",  &filename,
                        cpEnd);
  if(res == 0){
    FILE *fp = fopen(filename.cc(), "r");
    if(fp == 0)
      return(errh->error("cannot open file %s", filename.cc()));
    char buf[512];
    while(fgets(buf, sizeof(buf), fp)){
      struct delta d;
      if(sscanf(buf, "%lf%lf%lf", &d.interval, &d.lat, &d.lon) == 3){
        _deltas.push_back(d);
      } else {
        fclose(fp);
        return(errh->error("cannot parse a line in file %s", filename.cc()));
      }
    }
    fclose(fp);
    if(_deltas.size() < 1)
      return(errh->error("no locations in file %s", filename.cc()));
    click_chatter("read %d deltas from %s",
                  _deltas.size(),
                  filename.cc());
  }
  return res;
}

// Pick a new place to move to, and a time by which we want
// to arrive there.
void
LocFromFile::choose_new_leg(double *nlat, double *nlon, double *nt)
{
  *nlat = _deltas[_next].lat;
  *nlon = _deltas[_next].lon;
  *nt = _t0 + _deltas[_next].interval;

  _next += 1;
  if(_next >= _deltas.size())
    _next = 0;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(LocFromFile)

#include "vector.cc"
template class Vector<LocFromFile::delta>;
