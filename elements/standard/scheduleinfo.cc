/*
 * scheduleinfo.{cc,hh} -- element stores schedule parameters
 * Benjie Chen, Eddie Kohler
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
#include "scheduleinfo.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"

ScheduleInfo::ScheduleInfo()
{
}

int
ScheduleInfo::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  // find _prefix, which includes slash
  int last_slash = id().find_right('/');
  if (last_slash >= 0)
    _prefix = id().substring(0, last_slash + 1);
  else
    _prefix = String();

  // check for an earlier ScheduleInfo with the same prefix
  const Vector<Element *> &ev = router()->elements();
  for (int i = 0; i < eindex(); i++)
    if (ScheduleInfo *si = (ScheduleInfo *)ev[i]->cast("ScheduleInfo"))
      if (_prefix == si->_prefix) {
	_active = false;
	return si->configure(conf, errh);
      }
  _active = true;
  
  // compile scheduling info
  for (int i = 0; i < conf.size(); i++) {
    Vector<String> parts;
    int mt;
    cp_spacevec(conf[i], parts);
    if (parts.size() == 0)
      /* empty argument OK */;
    else if (parts.size() != 2 || !cp_real2(parts[1], FRAC_BITS, &mt))
      errh->error("expected `ELEMENTNAME PARAM', got `%s'", String(conf[i]).cc());
    else {
      for (int j = 0; j < _element_names.size(); j++)
	if (_element_names[j] == parts[0]) {
	  if (_max_tickets[j] != mt)
	    errh->error("conflicting ScheduleInfo for `%s'", parts[0].cc());
	  goto appended;
	}
      _element_names.push_back(parts[0]);
      _max_tickets.push_back(mt);
     appended: ;
    }
  }
  
  return 0;
}

bool
ScheduleInfo::query(const String &text, int &max_tickets) const
{
  for (int i = 0; i < _element_names.size(); i++)
    if (_element_names[i] == text) {
      max_tickets = _max_tickets[i];
      return true;
    }
  return false;
}

bool
ScheduleInfo::query_prefixes(const String &id, int &scaled_max_tickets,
			     String &output_id) const
{
  // Check `id', then `id's prefix, then its prefix, and so forth.
  int max_tickets;
  String text = id;
  while (text) {
    if (query(text, max_tickets)) {
      scaled_max_tickets =
	((long long)scaled_max_tickets * max_tickets) >> FRAC_BITS;
      output_id = id.substring(text.length() + 1);
      return true;
    }
    int slash = text.find_right('/');
    text = text.substring(0, (slash < 0 ? 0 : slash));
  }
  return false;
}

int
ScheduleInfo::query(Element *e, ErrorHandler *errh)
{
  const Vector<Element *> &ev = e->router()->elements();
  Vector<Element *> schinfos;
  for (int i = 0; i < ev.size(); i++)
    if (ScheduleInfo *e = (ScheduleInfo *)ev[i]->cast("ScheduleInfo"))
      if (e->_active)
	schinfos.push_back(e);

  // check prefixes in order of increasing length
  String id = e->id();
  String text = id;
  int max_tickets = DEFAULT;
  int slash;
  while (text) {

    // find current prefix, which includes slash
    String prefix = id.substring(0, id.length() - text.length());
    bool warning = false;	// only warn if there was partial ScheduleInfo
    
    for (int i = 0; i < schinfos.size(); i++) {
      ScheduleInfo *e = (ScheduleInfo *)schinfos[i];
      if (e->_prefix == prefix) {
	if (e->query_prefixes(text, max_tickets, text))
	  goto found;
	warning = true;
      }
    }

    slash = text.find_left('/');
    if (slash >= 0) {
      if (warning)
	errh->warning("no ScheduleInfo for compound element `%s'",
		      text.substring(0, slash).cc());
      text = text.substring(slash + 1);
    } else {
      if (warning)
	errh->warning("no ScheduleInfo for element `%s'", text.cc());
      text = String();
    }
    
   found: ;
  }

  // check for too many tickets
  if (max_tickets > ElementLink::MAX_TICKETS) {
    max_tickets = ElementLink::MAX_TICKETS;
    String m = cp_unparse_real(max_tickets, FRAC_BITS);
    errh->warning("ScheduleInfo too high; reduced to %s", m.cc());
  }
  
  // return the result you've got
  return max_tickets;
}

void
ScheduleInfo::join_scheduler(Element *e, ErrorHandler *errh)
{
#if !RR_SCHED
  int max_tickets = query(e, errh);
  e->set_max_tickets(max_tickets);
  e->set_tickets(max_tickets);
#endif
  e->join_scheduler();
}

EXPORT_ELEMENT(ScheduleInfo)
