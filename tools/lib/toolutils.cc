/*
 * toolutils.{cc,hh} -- utility routines for tools
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
#include "straccum.hh"
#include "bitvector.hh"
#include "routert.hh"
#include "lexert.hh"
#include "toolutils.hh"
#include "confparse.hh"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <stdarg.h>

bool ignore_line_directives = false;

RouterT *
read_router_file(const char *filename, ErrorHandler *errh, RouterT *router)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();

  // read file string
  int old_nerrors = errh->nerrors();
  String s = file_string(filename, errh);
  if (!s && errh->nerrors() != old_nerrors)
    return 0;

  // check for archive
  Vector<ArchiveElement> archive;
  if (s.length() && s[0] == '!') {
    separate_ar_string(s, archive, errh);
    int found = -1;
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].name == "config")
	found = i;
    if (found >= 0)
      s = archive[found].data;
    else {
      errh->error("archive has no `config' section");
      s = String();
    }
  }

  // read router
  if (!filename || strcmp(filename, "-") == 0)
    filename = "<stdin>";
  LexerT lexer(errh, ignore_line_directives);
  lexer.reset(s, filename);
  if (router)
    lexer.set_router(router);
  while (lexer.ystatement()) ;
  router = lexer.take_router();

  // add archive bits
  if (router && archive.size()) {
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].live() && archive[i].name != "config")
	router->add_archive(archive[i]);
  }

  // done
  return router;
}

void
write_router_file(RouterT *r, FILE *f, ErrorHandler *errh)
{
  if (!r) return;
  
  String config_str = r->configuration_string();
  
  // create archive if necessary
  const Vector<ArchiveElement> &archive = r->archive();
  if (archive.size()) {
    Vector<ArchiveElement> narchive;
    
    // add configuration
    ArchiveElement config_ae;
    config_ae.name = "config";
    config_ae.date = time(0);
    config_ae.uid = geteuid();
    config_ae.gid = getegid();
    config_ae.mode = 0644;
    config_ae.data = config_str;
    narchive.push_back(config_ae);
    
    // add other archive elements
    for (int i = 0; i < archive.size(); i++)
      if (archive[i].live() && archive[i].name != "config")
	narchive.push_back(archive[i]);

    if (narchive.size() > 1)
      config_str = create_ar_string(narchive, errh);
  }
  
  fwrite(config_str.data(), 1, config_str.length(), f);
}

int
write_router_file(RouterT *r, const char *name, ErrorHandler *errh)
{
  if (name && strcmp(name, "-") != 0) {
    FILE *f = fopen(name, "wb");
    if (!f) {
      if (errh)
	errh->error("%s: %s", name, strerror(errno));
      return -1;
    }
    write_router_file(r, f, errh);
    fclose(f);
  } else
    write_router_file(r, stdout, errh);
  return 0;
}

// ELEMENTMAP

ElementMap::ElementMap()
  : _name_map(0), _cxx_name_map(0), _requirement_map(-1), _nrequirements(0)
{
  (void) requirement("linuxmodule");
  _requirement_map.insert("userlevel", 1);
  _requirement_map.insert("!userlevel", 0);
  _requirement_name[1] = "userlevel";
  add(String(), String(), String(), String());
}

ElementMap::ElementMap(const String &str)
  : _name_map(0), _cxx_name_map(0), _requirement_map(-1), _nrequirements(0)
{
  (void) requirement("linuxmodule");
  _requirement_map.insert("userlevel", 1);
  _requirement_map.insert("!userlevel", 0);
  _requirement_name[1] = "userlevel";
  add(String(), String(), String(), String());
  parse(str);
}

ElementMap::~ElementMap()
{
  for (int i = 0; i < _name.size(); i++) {
    delete _requirements[i];
    delete _provisions[i];
  }
}

String
ElementMap::processing_code(const String &n) const
{
  return _processing_code[ _name_map[n] ];
}

int
ElementMap::requirement(const String &s)
{
  bool negated = (s.length() && s[0] == '!');
  String req = (negated ? s.substring(1) : s);
  int i = _requirement_map[req];
  if (i < 0) {
    i = _nrequirements;
    _nrequirements += 2;
    _requirement_map.insert(req, i);
    _requirement_name.push_back(req);
    _requirement_name.push_back("!" + req);    
  }
  return i ^ (negated ? 1 : 0);
}

int
ElementMap::add(const String &click_name, String cxx_name,
		String header_file, String processing_code)
{
  if (!click_name && !cxx_name)
    return -1;

  if (cxx_name == "?")
    cxx_name = String();
  if (header_file == "?")
    header_file = String();
  if (processing_code == "?")
    processing_code = String();
  
  int old_name = _name_map[click_name];
  int old_cxx_name = _cxx_name_map[cxx_name];

  _name.push_back(click_name);
  _cxx_name.push_back(cxx_name);
  _header_file.push_back(header_file);
  _processing_code.push_back(processing_code);
  _requirements.push_back(0);
  _provisions.push_back(0);

  int i = _name.size() - 1;
  _name_map.insert(click_name, i);
  _cxx_name_map.insert(cxx_name, i);
  _name_next.push_back(old_name);
  _cxx_name_next.push_back(old_cxx_name);
}

void
ElementMap::add_requirement(int i, int r)
{
  Bitvector *b = _requirements[i];
  if (!b)
    b = _requirements[i] = new Bitvector(_nrequirements);
  b->force_bit(r) = true;
}

void
ElementMap::add_provision(int i, int r)
{
  Bitvector *b = _provisions[i];
  if (!b)
    b = _provisions[i] = new Bitvector(_nrequirements);
  b->force_bit(r) = true;
}

void
ElementMap::remove(int i)
{
  if (i <= 0 || i >= _name.size())
    return;

  int p = -1;
  for (int t = _name_map[ _name[i] ]; t > 0; p = t, t = _name_next[t])
    /* nada */;
  if (p >= 0)
    _name_next[p] = _name_next[i];
  else
    _name_map.insert(_name[i], _name_next[i]);

  p = -1;
  for (int t = _cxx_name_map[ _cxx_name[i] ]; t > 0; p = t, t = _cxx_name_next[t])
    /* nada */;
  if (p >= 0)
    _cxx_name_next[p] = _cxx_name_next[i];
  else
    _cxx_name_map.insert(_cxx_name[i], _cxx_name_next[i]);

  _name[i] = String();
  _cxx_name[i] = String();
}

void
ElementMap::parse(const String &str)
{
  Vector<String> name, cxx_name, header, processing, requirements, provisions;
  parse_tabbed_lines(str, &name, &cxx_name, &header, &processing,
		     &requirements, &provisions, (void *)0);
  for (int i = 0; i < name.size(); i++) {
    int which = add(name[i], cxx_name[i], header[i], processing[i]);
    if (which < 0)
      continue;
    if (requirements[i]) {
      Vector<String> words;
      cp_spacevec(requirements[i], words);
      for (int j = 0; j < words.size(); j++)
	add_requirement(which, requirement(words[j]));
    }
    if (provisions[i]) {
      Vector<String> words;
      cp_spacevec(requirements[i], words);
      for (int j = 0; j < words.size(); j++)
	add_provision(which, requirement(words[j]));
    }
  }
}

void
ElementMap::unparse_requirements(Bitvector *b, StringAccum &sa) const
{
  bool any = false;
  sa << '"';
  for (int j = 0; j < b->size(); j++)
    if ((*b)[j]) {
      sa << (any ? " " : "") << _requirement_name[j];
      any = true;
    }
  sa << '"';
}

String
ElementMap::unparse() const
{
  StringAccum sa;
  for (int i = 1; i < size(); i++) {
    if (!_name[i] && !_cxx_name[i])
      continue;
    if (_name[i])
      sa << _name[i] << '\t';
    else
      sa << "\"\"\t";
    if (_cxx_name[i])
      sa << _cxx_name[i] << '\t';
    else
      sa << "?\t";
    if (_header_file[i])
      sa << _header_file[i] << '\t';
    else
      sa << "?\t";
    if (_processing_code[i])
      sa << _processing_code[i] << '\t';
    else
      sa << "?\t";
    if (_requirements[i])
      unparse_requirements(_requirements[i], sa);
    else
      sa << "\"\"";
    sa << '\t';
    if (_provisions[i])
      unparse_requirements(_provisions[i], sa);
    else
      sa << "\"\"";
    sa << '\n';
  }
  return sa.take_string();
}
