/*
 * spinlockinfo.{cc,hh} -- element stores spinlocks
 * Benjie Chen
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include <click/config.h>
#include "spinlockinfo.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>

SpinlockInfo::SpinlockInfo()
  : _map(-1)
{
}

SpinlockInfo::~SpinlockInfo()
{
  uninitialize();
}

void
SpinlockInfo::uninitialize()
{
  _map.clear();
  for (int i=0; i<_spinlocks.size(); i++)
    _spinlocks[i]->unref();
  _spinlocks.clear();
}

int
SpinlockInfo::add_spinlock(const Vector<String> &conf,
                           const String &prefix,
			   ErrorHandler *errh)
{
  int before = errh->nerrors();
  for (int i = 0; i < conf.size(); i++) {
    Vector<String> parts;
    cp_spacevec(conf[i], parts);
    if (parts.size() != 1)
      errh->error("expected `NAME'");
    else {
      String name = prefix + parts[0];
      if (_map[name] < 0) {
	Spinlock *l = new Spinlock();
	l->ref();
	_map.insert(name, _spinlocks.size());
	_spinlocks.push_back(l);
      }
    }
  }
  return (errh->nerrors() == before ? 0 : -1);
}

int
SpinlockInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  // find prefix, which includes slash
  String prefix;
  int last_slash = id().find_right('/');
  if (last_slash >= 0)
    prefix = id().substring(0, last_slash + 1);
  else
    prefix = String();

  // put everything in the first SpinlockInfo
  const Vector<Element *> &ev = router()->elements();
  for (int i = 0; i <= eindex(); i++)
    if (SpinlockInfo *si = (SpinlockInfo *)ev[i]->cast("SpinlockInfo"))
      return si->add_spinlock(conf, prefix, errh);

  // should never get here
  return -1;
}

Spinlock*
SpinlockInfo::query(const String &name, 
		    const String &eid) const
{
  String prefix = eid;
  int slash = prefix.find_right('/');
  prefix = prefix.substring(0, (slash < 0 ? 0 : slash + 1));
  
  while (1) {
    int e = _map[prefix + name];
    if (e >= 0) return _spinlocks[e];
    else if (!prefix)
      return 0;
    slash = prefix.find_right('/', prefix.length() - 2);
    prefix = prefix.substring(0, (slash < 0 ? 0 : slash + 1));
  }
}

EXPORT_ELEMENT(SpinlockInfo)

// template instance
#include <click/vector.cc>

