/*
 * click-align.cc -- alignment enforcer for Click configurations
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
#include "error.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "lexert.hh"
#include "routert.hh"
#include "alignment.hh"
#include "alignclass.hh"
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

  bool calc_input();
  void calc_output();
  
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
RouterAlign::calc_output()
{
  int ne = _icount.size();
  for (int i = 0; i < ne; i++)
    _aligners[i]->flow(_ialign, _ioffset[i], _icount[i],
		       _oalign, _ooffset[i], _ocount[i]);
}

bool
RouterAlign::calc_input()
{
  const Vector<Hookup> &hf = _router->hookup_from();
  const Vector<Hookup> &ht = _router->hookup_to();
  int nh = hf.size();
  int ne = _icount.size();
  
  Vector<Alignment> new_ialign(ne, Alignment());
  for (int i = 0; i < nh; i++) {
    int ioff = _ioffset[ht[i].idx] + ht[i].port;
    int ooff = _ooffset[hf[i].idx] + hf[i].port;
    new_ialign[ioff] *= _oalign[ooff];
  }

  // see if anything happened
  bool changed = false;
  for (int i = 0; i < ne && !changed; i++)
    if (new_ialign[i] != _ialign[i])
      changed = true;
  _ialign.swap(new_ialign);
  return changed;
}


String
read_file_string(const char *filename, ErrorHandler *)
{
  FILE *f;
  if (filename && strcmp(filename, "-") != 0) {
    f = fopen(filename, "r");
    if (!f) return String();
  } else {
    f = stdin;
    filename = "<stdin>";
  }

  StringAccum sa;
  char buf[4096];
  while (!feof(f)) {
    size_t r = fread(buf, 1, 4096, f);
    if (r > 0)
      memcpy(sa.extend(r), buf, r);
  }
  
  if (f != stdin) fclose(f);
  return cp_subst(sa.take_string());
}
  

void
prepare_router(RouterT *r)
{
  r->get_type_index("Align", new AlignAlignClass);
  r->get_type_index("Strip", new StripAlignClass);
  r->get_type_index("FromDevice", new AlignClass(new GeneratorAligner(Alignment(4, 2))));
  r->get_type_index("CheckIPHeader", new AlignClass(new WantAligner(Alignment(4, 0))));
}

RouterT *
read_router_file(const char *filename, ErrorHandler *errh)
{
  FILE *f;
  if (filename && strcmp(filename, "-") != 0) {
    f = fopen(filename, "r");
    if (!f) return 0;
  } else {
    f = stdin;
    filename = "<stdin>";
  }
  
  FileLexerTSource lex_source(filename, f);
  LexerT lexer(errh);
  lexer.reset(&lex_source);
  prepare_router(lexer.router());
  while (lexer.ystatement()) ;
  RouterT *r = lexer.take_router();

  if (f != stdin) fclose(f);
  return r;
}
  

static void
print_router_align(const RouterAlign &ral)
{
  RouterT *router = ral._router;
  int ne = router->nelements();
  for (int i = 0; i < ne; i++) {
    fprintf(stderr, "%s :", router->ename(i).cc());
    for (int j = 0; j < ral._icount[i]; j++) {
      const Alignment &a = ral._ialign[ ral._ioffset[i] + j ];
      fprintf(stderr, " %d/%d", a.chunk(), a.offset());
    }
    fprintf(stderr, " /");
    for (int j = 0; j < ral._ocount[i]; j++) {
      const Alignment &a = ral._oalign[ ral._ooffset[i] + j ];
      fprintf(stderr, " %d/%d", a.chunk(), a.offset());
    }
    fprintf(stderr, "\n");
  }
}

inline String
aligner_name(int anonymizer)
{
  return String("@click_align@") + String(anonymizer);
}

int
main(int argc, char **argv)
{
  String::static_initialize();
  ErrorHandler::static_initialize(new FileErrorHandler(stderr));
  ErrorHandler *errh = ErrorHandler::default_handler();

  RouterT *router = read_router_file(argv[1], errh);
  if (!router) exit(1);
  router->flatten(errh);
  int align_tindex = router->get_type_index("Align");

  int original_nelements = router->nelements();
  int anonymizer = original_nelements + 1;
  while (router->eindex(aligner_name(anonymizer)) >= 0)
    anonymizer++;
  
  {
    // calculate current alignment
    RouterAlign ral(router, errh);
    do {
      ral.calc_output();
    } while (ral.calc_input());
    
    // add required aligns
    for (int i = 0; i < original_nelements; i++)
      for (int j = 0; j < ral._icount[i]; j++) {
	Alignment have = ral._ialign[ ral._ioffset[i] + j ];
	Alignment want = ral._aligners[i]->want(j);
	if (have <= want)
	  /* do nothing */;
	else {
	  int ei = router->get_eindex
	    (aligner_name(anonymizer), align_tindex,
	     String(want.chunk()) + ", " + String(want.offset()));
	  router->insert_before(ei, Hookup(i, j));
	  anonymizer++;
	}
      }
  }

  // remove redundant Aligns

  while (1) {
    // calculate current alignment
    RouterAlign ral(router, errh);
    do {
      ral.calc_output();
    } while (ral.calc_input());

    // skip redundant Aligns
    bool changed = false;
    Vector<Hookup> &hf = router->hookup_from();
    Vector<Hookup> &ht = router->hookup_to();
    int nhook = hf.size();
    for (int i = 0; i < nhook; i++)
      if (router->etype(ht[i].idx) == align_tindex) {
	Alignment have = ral._oalign[ ral._ooffset[hf[i].idx] + hf[i].port ];
	Alignment want = ral._oalign[ ral._ooffset[ht[i].idx] ];
	if (have <= want) {
	  changed = true;
	  Vector<Hookup> align_dest;
	  router->find_connections_from(ht[i], align_dest);
	  for (int j = 0; j < align_dest.size(); j++)
	    router->add_connection(hf[i], align_dest[j]);
	  ht[i].idx = -1;
	}
      }

    if (!changed) break;
    
    router->remove_bad_connections();
    router->remove_duplicate_connections();
  }
  
    
  RouterAlign ral(router, errh);
  do {
    ral.calc_output();
  } while (ral.calc_input());
  print_router_align(ral);
  fputs(router->configuration_string(), stderr);
  return 0;
}
