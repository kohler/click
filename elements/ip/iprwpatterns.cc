/*
 * iprwpatterns.{cc,hh} -- stores shared IPRewriter patterns
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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

#include <click/config.h>
#include "iprwpatterns.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>

IPRewriterPatterns::IPRewriterPatterns()
  : _name_map(-1)
{
  MOD_INC_USE_COUNT;
}

IPRewriterPatterns::~IPRewriterPatterns()
{
  MOD_DEC_USE_COUNT;
}

int
IPRewriterPatterns::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  // check for an earlier IPRewriterPatterns
  const Vector<Element *> &ev = router()->elements();
  for (int i = 0; i < eindex(); i++)
    if (IPRewriterPatterns *rwp = (IPRewriterPatterns *)ev[i]->cast("IPRewriterPatterns"))
      return rwp->configure(conf, errh);

  for (int i = 0; i < conf.size(); i++) {
    String word, rest;
    // allow empty patterns for convenience
    if (!cp_word(conf[i], &word, &rest))
      continue;
    cp_eat_space(rest);

    if (_name_map[word] >= 0) {
      errh->error("pattern name `%s' has already been defined", word.cc());
      continue;
    }

    IPRw::Pattern *p;
    if (IPRw::Pattern::parse(rest, &p, this, errh) >= 0) {
      p->use();
      _name_map.insert(word, _patterns.size());
      _patterns.push_back(p);
    }
  }
  return 0;
}

void
IPRewriterPatterns::uninitialize()
{
  for (int i = 0; i < _patterns.size(); i++)
    _patterns[i]->unuse();
}

IPRw::Pattern *
IPRewriterPatterns::find(Element *e, const String &name, ErrorHandler *errh)
{
  const Vector<Element *> &ev = e->router()->elements();
  for (int i = 0; i < ev.size(); i++)
    if (IPRewriterPatterns *rwp = (IPRewriterPatterns *)ev[i]->cast("IPRewriterPatterns")) {
      int x = rwp->_name_map[name];
      if (x >= 0)
	return rwp->_patterns[x];
      break;
    }
  errh->error("no pattern named `%s'", String(name).cc());
  return 0;
}

ELEMENT_REQUIRES(IPRw)
EXPORT_ELEMENT(IPRewriterPatterns)
