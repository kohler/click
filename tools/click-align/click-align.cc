/*
 * click-align.cc -- alignment enforcer for Click configurations
 * Eddie Kohler
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
#include "error.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "lexert.hh"
#include "routert.hh"
#include "alignment.hh"
#include "alignclass.hh"
#include "toolutils.hh"
#include "clp.h"
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

  bool have_input();
  void have_output();
  
  void want_input();
  bool want_output();
  
  void print(FILE *);
  
};

RouterAlign::RouterAlign(RouterT *r, ErrorHandler *errh)
{
  _router = r;
  _router->count_ports(_icount, _ocount);
  int ne = _icount.size();
  int id = 0, od = 0;
  for (int i = 0; i < ne; i++) {
    _ioffset.push_back(id);
    _ooffset.push_back(od);
    id += _icount[i];
    od += _ocount[i];
  }
  // set alignments
  _ialign.assign(id, Alignment());
  _oalign.assign(od, Alignment());
  // find aligners
  for (int i = 0; i < ne; i++) {
    AlignClass *eclass = (AlignClass *)_router->etype_class(i);
    Aligner *aligner = 0;
    if (eclass)
      aligner = eclass->create_aligner(_router->element(i), _router, errh);
    if (!aligner)
      aligner = default_aligner();
    _aligners.push_back(aligner);
  }
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
  const Vector<Hookup> &hf = _router->hookup_from();
  const Vector<Hookup> &ht = _router->hookup_to();
  int nh = hf.size();
  int nialign = _ialign.size();

  Vector<Alignment> new_ialign(nialign, Alignment());
  for (int i = 0; i < nh; i++)
    if (hf[i].idx >= 0) {
      int ioff = _ioffset[ht[i].idx] + ht[i].port;
      int ooff = _ooffset[hf[i].idx] + hf[i].port;
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
  const Vector<Hookup> &hf = _router->hookup_from();
  const Vector<Hookup> &ht = _router->hookup_to();
  int nh = hf.size();
  int noalign = _oalign.size();

  Vector<Alignment> new_oalign(noalign, Alignment());
  for (int i = 0; i < nh; i++)
    if (hf[i].idx >= 0) {
      int ioff = _ioffset[ht[i].idx] + ht[i].port;
      int ooff = _ooffset[hf[i].idx] + hf[i].port;
      new_oalign[ooff] &= _ialign[ioff];
    }
  for (int i = 0; i < noalign; i++)
    if (new_oalign[i].bad())
      new_oalign[i] = Alignment();

  // see if anything happened
  bool changed = false;
  for (int i = 0; i < noalign && !changed; i++)
    if (new_oalign[i] != _oalign[i])
      changed = true;
  _oalign.swap(new_oalign);
  return changed;
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
    fprintf(f, " /");
    for (int j = 0; j < _ocount[i]; j++) {
      const Alignment &a = _oalign[ _ooffset[i] + j ];
      fprintf(f, " %d/%d", a.chunk(), a.offset());
    }
    fprintf(f, "\n");
  }
  fprintf(f, "\n");
}


RouterT *
prepared_router()
{
  RouterT *r = new RouterT;
  r->get_type_index("Align", new AlignAlignClass);
  r->get_type_index("Strip", new StripAlignClass);
  r->get_type_index("Classifier", new AlignClass(new ClassifierAligner));
  r->get_type_index("EtherEncap", new AlignClass(new ShifterAligner(-14)));
  Aligner *a = new GeneratorAligner(Alignment(4, 2));
  r->get_type_index("FromDevice", new AlignClass(a));
  r->get_type_index("PollDevice", new AlignClass(a));
  r->get_type_index("FromLinux", new AlignClass(a));
  a = new GeneratorAligner(Alignment(4, 0));
  r->get_type_index("RatedSource", new AlignClass(a));
  a = new WantAligner(Alignment(4, 2));
  r->get_type_index("ToLinux", new AlignClass(a));
  a = new WantAligner(Alignment(4, 0));
  r->get_type_index("CheckIPHeader", new AlignClass(a));
  r->get_type_index("CheckIPHeader2", new AlignClass(a));
  r->get_type_index("MarkIPHeader", new AlignClass(a));
  r->get_type_index("IPEncap", new AlignClass(a));
  r->get_type_index("UDPIPEncap", new AlignClass(a));
  r->get_type_index("ICMPPingEncap", new AlignClass(a));
  r->get_type_index("RandomUDPIPEncap", new AlignClass(a));
  r->get_type_index("RoundRobinUDPIPEncap", new AlignClass(a));
  r->get_type_index("RoundRobinTCPIPEncap", new AlignClass(a));
  a = new CombinedAligner(new ShifterAligner(14), new WantAligner(Alignment(4, 2)));
  r->get_type_index("IPInputCombo", new AlignClass(a));
  a = new WantAligner(Alignment(2, 0));
  r->get_type_index("ARPResponder", new AlignClass(a));
  r->get_type_index("ARPQuerier", new AlignClass(a));
  return r;
}


#define HELP_OPT		300
#define VERSION_OPT		301
#define ROUTER_OPT		302
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
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();

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
      printf("click-align (Click) %s\n", VERSION);
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
  RouterT *router = read_router_file(router_file, errh, prepared_router());
  if (!router || errh->nerrors() > 0)
    exit(1);
  router->flatten(errh);
  int align_tindex = router->type_index("Align");

  int original_nelements = router->nelements();
  int anonymizer = original_nelements + 1;
  while (router->eindex(aligner_name(anonymizer)) >= 0)
    anonymizer++;
  
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
	if (have <= want)
	  /* do nothing */;
	else {
	  int ei = router->get_eindex
	    (aligner_name(anonymizer), align_tindex,
	     String(want.chunk()) + ", " + String(want.offset()),
	     "<click-align>");
	  router->insert_before(ei, Hookup(i, j));
	  anonymizer++;
	}
      }
  }

  // remove useless Aligns
  {
    const Vector<Hookup> &hf = router->hookup_from();
    const Vector<Hookup> &ht = router->hookup_to();
    int nhook = hf.size();
    for (int i = 0; i < nhook; i++)
      if (hf[i].idx >= 0
	  && router->etype(ht[i].idx) == align_tindex
	  && router->etype(hf[i].idx) == align_tindex) {
	// skip over hf[i]
	Vector<Hookup> above, below;
	router->find_connections_to(hf[i], above);
	router->find_connections_from(hf[i], below);
	if (below.size() == 1) {
	  for (int j = 0; j < nhook; j++)
	    if (ht[j] == hf[i])
	      router->change_connection_to(j, ht[i]);
	} else if (above.size() == 1) {
	  for (int j = 0; j < nhook; j++)
	    if (ht[j] == ht[i])
	      router->change_connection_from(j, above[0]);
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
    const Vector<Hookup> &hf = router->hookup_from();
    const Vector<Hookup> &ht = router->hookup_to();
    int nhook = hf.size();
    for (int i = 0; i < nhook; i++)
      if (hf[i].idx >= 0 && router->etype(ht[i].idx) == align_tindex) {
	Alignment have = ral._oalign[ ral._ooffset[hf[i].idx] + hf[i].port ];
	Alignment want = ral._oalign[ ral._ooffset[ht[i].idx] ];
	if (have <= want) {
	  changed = true;
	  Vector<Hookup> align_dest;
	  router->find_connections_from(ht[i], align_dest);
	  for (int j = 0; j < align_dest.size(); j++)
	    router->add_connection(hf[i], align_dest[j]);
	  router->kill_connection(i);
	}
      }

    if (!changed) break;
    
    router->remove_duplicate_connections();
  }

  // remove unused Aligns (they have no input)
  {
    Vector<int> ninputs, noutputs;
    router->count_ports(ninputs, noutputs);
    int nelem = router->nelements();
    for (int i = 0; i < nelem; i++)
      if (router->etype(i) == align_tindex
	  && (ninputs[i] == 0 || noutputs[i] == 0))
	router->kill_element(i);
    router->remove_dead_elements();
  }

  // make the AlignmentInfo element
  {
    RouterAlign ral(router, errh);
    do {
      ral.have_output();
    } while (ral.have_input());

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

    int aligninfo_t = router->get_type_index("AlignmentInfo", 0);
    router->get_eindex(String("AlignmentInfo@click_align@") + String(nelem+1),
		       aligninfo_t,
		       sa.take_string(),
		       "<click-align>");
  }
  
  // write result
  if (write_router_file(router, output_file, errh) < 0)
    exit(1);
  return 0;
}

// generate Vector template instance
#include "vector.cc"
template class Vector<Alignment>;
