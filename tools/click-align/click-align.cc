/*
 * click-align.cc -- alignment enforcer for Click configurations
 * Eddie Kohler
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

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/driver.hh>
#include "lexert.hh"
#include "routert.hh"
#include "alignment.hh"
#include "alignclass.hh"
#include "toolutils.hh"
#include <click/clp.h>
#include <stdio.h>
#include <ctype.h>

struct RouterAlign {

  RouterT *_router;
  Vector<int> _icount;
  Vector<int> _ocount;
  Vector<int> _ioffset;
  Vector<int> _ooffset;
  Vector<Alignment> _ialign;
  Vector<Alignment> _oalign;
  Vector<Aligner *> _aligners;

  RouterAlign(RouterT *, ErrorHandler *);

  int iindex_eindex(int) const;
  int iindex_port(int) const;
  int oindex_eindex(int) const;
  int oindex_port(int) const;
  
  bool have_input();
  void have_output();
  
  void want_input();
  bool want_output();

  void adjust();
  
  void print(FILE *);
  
};

RouterAlign::RouterAlign(RouterT *r, ErrorHandler *errh)
{
  _router = r;
  int ne = r->nelements();
  int id = 0, od = 0;
  for (int i = 0; i < ne; i++) {
    _ioffset.push_back(id);
    _ooffset.push_back(od);
    ElementT *e = r->elt(i);
    _icount.push_back(e->ninputs());
    _ocount.push_back(e->noutputs());
    id += e->ninputs();
    od += e->noutputs();
  }
  _ioffset.push_back(id);
  _ooffset.push_back(od);
  // set alignments
  _ialign.assign(id, Alignment());
  _oalign.assign(od, Alignment());
  // find aligners
  for (int i = 0; i < ne; i++) {
    AlignClass *eclass = (AlignClass *)_router->etype(i)->cast("AlignClass");
    Aligner *aligner = 0;
    if (eclass)
      aligner = eclass->create_aligner(_router->element(i), _router, errh);
    if (!aligner)
      aligner = default_aligner();
    _aligners.push_back(aligner);
  }
}

int
RouterAlign::iindex_eindex(int ii) const
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    if (ii < _ioffset[i+1])
      return i;
  return -1;
}

int
RouterAlign::iindex_port(int ii) const
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    if (ii < _ioffset[i+1])
      return ii - _ioffset[i];
  return -1;
}

int
RouterAlign::oindex_eindex(int oi) const
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    if (oi < _ooffset[i+1])
      return i;
  return -1;
}

int
RouterAlign::oindex_port(int oi) const
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    if (oi < _ooffset[i+1])
      return oi - _ooffset[i];
  return -1;
}

void
RouterAlign::have_output()
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    _aligners[i]->have_flow(_ialign, _ioffset[i], _icount[i],
			    _oalign, _ooffset[i], _ocount[i]);
}

bool
RouterAlign::have_input()
{
  const Vector<ConnectionT> &conn = _router->connections();
  int nh = conn.size();
  int nialign = _ialign.size();

  Vector<Alignment> new_ialign(nialign, Alignment());
  for (int i = 0; i < nh; i++)
    if (conn[i].live()) {
      int ioff = _ioffset[conn[i].to_idx()] + conn[i].to_port();
      int ooff = _ooffset[conn[i].from_idx()] + conn[i].from_port();
      new_ialign[ioff] |= _oalign[ooff];
    }

  // see if anything happened
  bool changed = false;
  for (int i = 0; i < nialign && !changed; i++)
    if (new_ialign[i] != _ialign[i])
      changed = true;
  _ialign.swap(new_ialign);
  return changed;
}

void
RouterAlign::want_input()
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    _aligners[i]->want_flow(_ialign, _ioffset[i], _icount[i],
			    _oalign, _ooffset[i], _ocount[i]);
}

bool
RouterAlign::want_output()
{
  const Vector<ConnectionT> &conn = _router->connections();
  int nh = conn.size();
  int noalign = _oalign.size();

  Vector<Alignment> new_oalign(noalign, Alignment());
  for (int i = 0; i < nh; i++)
    if (conn[i].live()) {
      int ioff = _ioffset[conn[i].to_idx()] + conn[i].to_port();
      int ooff = _ooffset[conn[i].from_idx()] + conn[i].from_port();
      new_oalign[ooff] &= _ialign[ioff];
    }
  /* for (int i = 0; i < noalign; i++)
    if (new_oalign[i].bad())
      new_oalign[i] = Alignment(); */

  // see if anything happened
  bool changed = false;
  for (int i = 0; i < noalign && !changed; i++)
    if (new_oalign[i] != _oalign[i]) {
      /* fprintf(stderr, "%s[%d] %s <- %s\n", _router->ename(oindex_eindex(i)).cc(), oindex_port(i), new_oalign[i].s().cc(), _oalign[i].s().cc()); */
      changed = true;
    }
  _oalign.swap(new_oalign);
  return changed;
}

void
RouterAlign::adjust()
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    _aligners[i]->adjust_flow(_ialign, _ioffset[i], _icount[i],
			      _oalign, _ooffset[i], _ocount[i]);
}

void
RouterAlign::print(FILE *f)
{
  int ne = _router->nelements();
  for (int i = 0; i < ne; i++) {
    fprintf(f, "%s :", _router->ename(i).cc());
    for (int j = 0; j < _icount[i]; j++) {
      const Alignment &a = _ialign[ _ioffset[i] + j ];
      fprintf(f, " %d/%d", a.chunk(), a.offset());
    }
    fprintf(f, " -");
    for (int j = 0; j < _ocount[i]; j++) {
      const Alignment &a = _oalign[ _ooffset[i] + j ];
      fprintf(f, " %d/%d", a.chunk(), a.offset());
    }
    fprintf(f, "\n");
  }
  fprintf(f, "\n");
}


void
prepare_classes()
{
  // specialized classes
  ElementClassT::set_default_class(new AlignAlignClass);
  ElementClassT::set_default_class(new StripAlignClass);
  ElementClassT::set_default_class(new CheckIPHeaderAlignClass("CheckIPHeader", 1));
  ElementClassT::set_default_class(new CheckIPHeaderAlignClass("CheckIPHeader2", 1));
  ElementClassT::set_default_class(new CheckIPHeaderAlignClass("MarkIPHeader", 0));
  ElementClassT::set_default_class(new AlignClass("Classifier", new ClassifierAligner));
  ElementClassT::set_default_class(new AlignClass("EtherEncap", new ShifterAligner(-14)));

  ElementClassT::set_default_class(new AlignClass("IPInputCombo",
	new CombinedAligner(new ShifterAligner(14),
			    new WantAligner(Alignment(4, 2)))));

  // no alignment requirements, and do not transmit packets between input and
  // output
  Aligner *a = new NullAligner;
  ElementClassT::set_default_class(new AlignClass("Idle", a));
  ElementClassT::set_default_class(new AlignClass("Discard", a));
  
  // generate alignment 4/2
  a = new GeneratorAligner(Alignment(4, 2));
  ElementClassT::set_default_class(new AlignClass("FromDevice", a));
  ElementClassT::set_default_class(new AlignClass("PollDevice", a));
  ElementClassT::set_default_class(new AlignClass("FromLinux", a));

  // generate alignment 4/0
  a = new GeneratorAligner(Alignment(4, 0));
  ElementClassT::set_default_class(new AlignClass("RatedSource", a));
  ElementClassT::set_default_class(new AlignClass("ICMPError", a));

  // want alignment 4/2
  a = new WantAligner(Alignment(4, 2));
  ElementClassT::set_default_class(new AlignClass("ToLinux", a));

  // want alignment 4/0
  a = new WantAligner(Alignment(4, 0));
  ElementClassT::set_default_class(new AlignClass("IPEncap", a));
  ElementClassT::set_default_class(new AlignClass("UDPIPEncap", a));
  ElementClassT::set_default_class(new AlignClass("ICMPPingEncap", a));
  ElementClassT::set_default_class(new AlignClass("RandomUDPIPEncap", a));
  ElementClassT::set_default_class(new AlignClass("RoundRobinUDPIPEncap", a));
  ElementClassT::set_default_class(new AlignClass("RoundRobinTCPIPEncap", a));

  // want alignment 2/0
  a = new WantAligner(Alignment(2, 0));
  ElementClassT::set_default_class(new AlignClass("ARPResponder", a));
  ElementClassT::set_default_class(new AlignClass("ARPQuerier", a));
}


#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		303
#define OUTPUT_OPT		304

static Clp_Option options[] = {
  { "file", 'f', ROUTER_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

void
short_usage()
{
  fprintf(stderr, "Usage: %s [OPTION]... [ROUTERFILE]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage()
{
  printf("\
`Click-align' adds any required `Align' elements to a Click router\n\
configuration. The resulting router will work on machines that don't allow\n\
unaligned accesses. Its configuration is written to the standard output.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -o, --output FILE             Write output to FILE.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
}


inline String
aligner_name(int anonymizer)
{
  return String("Align@click_align@") + String(anonymizer);
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  cp_va_static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();
  CLICK_DEFAULT_PROVIDES;

  // read command line arguments
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = Clp_ProgramName(clp);

  const char *router_file = 0;
  const char *output_file = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {

     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("click-align (Click) %s\n", CLICK_VERSION);
      printf("Copyright (C) 1999 Massachusetts Institute of Technology\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case Clp_NotOption:
     case ROUTER_OPT:
      if (router_file) {
	errh->error("router file specified twice");
	goto bad_option;
      }
      router_file = clp->arg;
      break;

     case OUTPUT_OPT:
      if (output_file) {
	errh->error("output file specified twice");
	goto bad_option;
      }
      output_file = clp->arg;
      break;
      
     bad_option:
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
     case Clp_Done:
      goto done;
      
    }
  }
  
 done:
  prepare_classes();
  RouterT *router = read_router_file(router_file, errh);
  if (router)
    router->flatten(errh);
  if (!router || errh->nerrors() > 0)
    exit(1);
  ElementClassT *align_class = router->get_type("Align");
  assert(align_class);

  int original_nelements = router->nelements();
  int anonymizer = original_nelements + 1;
  while (router->eindex(aligner_name(anonymizer)) >= 0)
    anonymizer++;

  /*
   * Add Align elements directly required
   */
  int num_aligns_added = 0;
  {
    // calculate current alignment
    RouterAlign ral(router, errh);
    do {
      ral.have_output();
    } while (ral.have_input());
    
    // calculate desired alignment
    RouterAlign want_ral(router, errh);
    do {
      want_ral.want_input();
    } while (want_ral.want_output());

    // add required aligns
    for (int i = 0; i < original_nelements; i++)
      for (int j = 0; j < ral._icount[i]; j++) {
	Alignment have = ral._ialign[ ral._ioffset[i] + j ];
	Alignment want = want_ral._ialign[ ral._ioffset[i] + j ];
	if (have <= want || want.bad())
	  /* do nothing */;
	else {
	  int ei = router->get_eindex
	    (aligner_name(anonymizer), align_class,
	     String(want.chunk()) + ", " + String(want.offset()),
	     "<click-align>");
	  router->insert_before(ei, HookupI(i, j));
	  anonymizer++;
	  num_aligns_added++;
	}
      }
  }

  // remove useless Aligns
  {
    const Vector<ConnectionT> &conn = router->connections();
    int nhook = conn.size();
    for (int i = 0; i < nhook; i++)
      if (conn[i].live()
	  && router->etype(conn[i].to_idx()) == align_class
	  && router->etype(conn[i].from_idx()) == align_class) {
	// skip over hf[i]
	Vector<Hookup> above, below;
	router->find_connections_to(conn[i].from(), above);
	router->find_connections_from(conn[i].from(), below);
	if (below.size() == 1) {
	  for (int j = 0; j < nhook; j++)
	    if (conn[j].to() == conn[i].from())
	      router->change_connection_to(j, conn[i].to());
	} else if (above.size() == 1) {
	  for (int j = 0; j < nhook; j++)
	    if (conn[j].to() == conn[i].to())
	      router->change_connection_from(j, above[0]);
	}
      }
  }

  /*
   * Add Aligns required for adjustment purposes
   */
  {
    // calculate current alignment
    RouterAlign ral(router, errh);
    do {
      ral.have_output();
    } while (ral.have_input());
    
    // calculate adjusted alignment
    RouterAlign want_ral(ral);
    want_ral.adjust();

    // add required aligns
    for (int i = 0; i < original_nelements; i++)
      for (int j = 0; j < ral._icount[i]; j++) {
	Alignment have = ral._ialign[ ral._ioffset[i] + j ];
	Alignment want = want_ral._ialign[ ral._ioffset[i] + j ];
	if (have <= want || want.bad())
	  /* do nothing */;
	else {
	  int ei = router->get_eindex
	    (aligner_name(anonymizer), align_class,
	     String(want.chunk()) + ", " + String(want.offset()),
	     "<click-align>");
	  router->insert_before(ei, HookupI(i, j));
	  anonymizer++;
	  num_aligns_added++;
	}
      }
  }

  while (1) {
    // calculate current alignment
    RouterAlign ral(router, errh);
    do {
      ral.have_output();
    } while (ral.have_input());

    bool changed = false;
    
    // skip redundant Aligns
    const Vector<ConnectionT> &conn = router->connections();
    int nhook = conn.size();
    for (int i = 0; i < nhook; i++)
      if (conn[i].live() && router->etype(conn[i].to_idx()) == align_class) {
	Alignment have = ral._oalign[ ral._ooffset[conn[i].from_idx()] + conn[i].from_port() ];
	Alignment want = ral._oalign[ ral._ooffset[conn[i].to_idx()] ];
	if (have <= want) {
	  changed = true;
	  Vector<Hookup> align_dest;
	  router->find_connections_from(conn[i].to(), align_dest);
	  for (int j = 0; j < align_dest.size(); j++)
	    router->add_connection(conn[i].from(), align_dest[j]);
	  router->kill_connection(i);
	}
      }

    if (!changed) break;
    
    router->remove_duplicate_connections();
  }

  // remove unused Aligns (they have no input) and old AlignmentInfos
  ElementClassT *aligninfo_class = router->get_type("AlignmentInfo");
  {
    for (RouterT::iterator x = router->first_element(); x; x++)
      if (x->type() == align_class && (x->ninputs() == 0 || x->noutputs() == 0)) {
	x->kill();
	num_aligns_added--;
      } else if (x->type() == aligninfo_class)
	x->kill();
    router->remove_dead_elements();
  }
  
  // make the AlignmentInfo element
  {
    RouterAlign ral(router, errh);
    do {
      ral.have_output();
    } while (ral.have_input());

    // collect alignment information, delete old AlignmentInfos
    StringAccum sa;
    int nelem = router->nelements();
    for (int i = 0; i < nelem; i++) {
      if (sa.length()) sa << ",\n  ";
      sa << router->ename(i);
      for (int j = 0; j < ral._icount[i]; j++) {
	const Alignment &a = ral._ialign[ ral._ioffset[i] + j ];
	sa << "  " << a.chunk() << " " << a.offset();
      }
    }
    
    router->get_eindex(String("AlignmentInfo@click_align@") + String(nelem+1),
		       aligninfo_class, sa.take_string(),
		       "<click-align>");
  }

  // warn if added aligns
  if (num_aligns_added > 0)
    errh->warning((num_aligns_added > 1 ? "added %d Align elements" : "added %d Align element"), num_aligns_added);
  
  // write result
  if (write_router_file(router, output_file, errh) < 0)
    exit(1);
  return 0;
}

// generate Vector template instance
#include <click/vector.cc>
template class Vector<Alignment>;
