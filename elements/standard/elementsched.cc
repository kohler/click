/*
 * alignmentinfo.{cc,hh} -- element stores alignment information
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "elementsched.hh"
#include "glue.hh"
#include "confparse.hh"
#include "router.hh"
#include "error.hh"

ElementSched::ElementSched()
{
}

int
ElementSched::configure(const String &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  for (int i = 0; i < args.size(); i++) {
    Vector<String> parts;
    cp_spacevec(args[i], parts);
    
    if (parts.size() == 0)
      errh->warning("empty configuration argument %d", i);
    
    else if (parts.size() != 2) 
      errh->error("expected `ELEMENTNAME SCHEDULING_PARAM'");
    
    else if (Element *e = router()->find(this, parts[0], errh)) {
      int c = -1;
      if (!cp_integer(parts[1], c)) errh->error("expected SCHEDULING_PARAM");
      e->set_ntickets(c);
    } else
      errh->warning("no such element `%s'", parts[0].cc());
  }
  return 0;
}

EXPORT_ELEMENT(ElementSched)

