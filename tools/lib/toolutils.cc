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
read_router_file(const char *filename, bool empty_ok, RouterT *router,
		 ErrorHandler *errh)
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
  if (!s.length() && !empty_ok)
    errh->warning("%s: empty configuration", filename);
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

RouterT *
read_router_file(const char *filename, bool empty_ok, ErrorHandler *errh)
{
  return read_router_file(filename, empty_ok, 0, errh);
}

RouterT *
read_router_file(const char *filename, RouterT *prepared_router, ErrorHandler *errh)
{
  return read_router_file(filename, false, prepared_router, errh);
}

RouterT *
read_router_file(const char *filename, ErrorHandler *errh)
{
  return read_router_file(filename, false, 0, errh);
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
  : _name_map(0), _cxx_map(0)
{
  add(String(), String(), String(), String(), String(), String());
}

ElementMap::ElementMap(const String &str)
  : _name_map(0), _cxx_map(0)
{
  add(String(), String(), String(), String(), String(), String());
  parse(str);
}

String
ElementMap::processing_code(const String &n) const
{
  return _processing_code[ _name_map[n] ];
}

void
ElementMap::set_driver(int i, const String &requirements)
{
  int inreq = requirements.find_left("linuxmodule");
  if (inreq >= 0)		// XXX linuxmodule as substring?
    _driver[i] = DRIVER_LINUXMODULE;
  inreq = requirements.find_left("userlevel");
  if (inreq >= 0)
    _driver[i] = DRIVER_USERLEVEL;
}

int
ElementMap::add(const String &click_name, const String &cxx_name,
		const String &header_file, const String &processing_code,
		const String &requirements, const String &provisions)
{
  if (!click_name && !cxx_name)
    return -1;

  int old_name = _name_map[click_name];
  int old_cxx = _cxx_map[cxx_name];

  int i = _name.size();
  _name.push_back(click_name);
  _cxx.push_back(cxx_name);
  _header_file.push_back(header_file);
  _processing_code.push_back(processing_code);
  _requirements.push_back(requirements);
  _provisions.push_back(provisions);
  _driver.push_back(-1);
  if (requirements)
    set_driver(i, requirements);
  
  _name_map.insert(click_name, i);
  _cxx_map.insert(cxx_name, i);
  _name_next.push_back(old_name);
  _cxx_next.push_back(old_cxx);
  
  return i;
}

int
ElementMap::add(const String &click_name, const String &cxx_name,
		const String &header_file, const String &processing_code)
{
  return add(click_name, cxx_name, header_file, processing_code,
	     String(), String());
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
  for (int t = _cxx_map[ _cxx[i] ]; t > 0; p = t, t = _cxx_next[t])
    /* nada */;
  if (p >= 0)
    _cxx_next[p] = _cxx_next[i];
  else
    _cxx_map.insert(_cxx[i], _cxx_next[i]);

  _name[i] = String();
  _cxx[i] = String();
}

void
ElementMap::parse(const String &str)
{
  Vector<String> name, cxx_name, header, processing, requirements, provisions;
  parse_tabbed_lines(str, &name, &cxx_name, &header, &processing,
		     &requirements, &provisions, (void *)0);
  for (int i = 0; i < name.size(); i++)
    (void) add(name[i], cxx_name[i], header[i], processing[i],
	       requirements[i], provisions[i]);
}

String
ElementMap::unparse() const
{
  StringAccum sa;
  for (int i = 1; i < size(); i++) {
    if (!_name[i] && !_cxx[i])
      continue;
    sa << cp_quote(_name[i]) << '\t'
       << cp_quote(_cxx[i]) << '\t'
       << cp_quote(_header_file[i]) << '\t'
       << cp_quote(_processing_code[i]) << '\t'
       << cp_quote(_requirements[i]) << '\t'
       << cp_quote(_provisions[i]) << '\n';
  }
  return sa.take_string();
}

void
ElementMap::map_indexes(const RouterT *r, Vector<int> &map_indexes,
			ErrorHandler *errh) const
{
  assert(r->is_flat());
  map_indexes.assign(r->ntypes(), -1);
  for (int i = 0; i < r->nelements(); i++) {
    int t = r->etype(i);
    if (t >= 0 && map_indexes[t] == -1) {
      int idx = _name_map[r->type_name(t)];
      if (idx <= 0) {
	if (errh)
	  errh->error("nothing known about element class `%s'", String(r->type_name(t)).cc());
	map_indexes[t] = -2;
      } else
	map_indexes[t] = idx;
    }
  }
}

bool
ElementMap::driver_indifferent(const Vector<int> &map_indexes) const
{
  for (int i = 0; i < map_indexes.size(); i++) {
    int idx = map_indexes[i];
    if (idx > 0 && _driver[idx] >= 0)
      return false;
  }
  return true;
}

bool
ElementMap::driver_compatible(const Vector<int> &map_indexes, int driver) const
{
  for (int i = 0; i < map_indexes.size(); i++) {
    int idx = map_indexes[i];
    if (idx <= 0 || _driver[idx] < 0)
      continue;
    bool any = false;
    while (idx > 0 && !any) {
      if (_driver[idx] < 0 || _driver[idx] == driver)
	any = true;
      else
	idx = _name_next[idx];
    }
    if (!any)
      return false;
  }
  return true;
}

void
ElementMap::limit_driver(int driver)
{
  for (HashMap<String, int>::Iterator i = _name_map.first(); i; i++) {
    int t = i.value();
    while (t > 0 && _driver[t] >= 0 && _driver[t] != driver)
      t = i.value() = _name_next[t];
  }
  for (HashMap<String, int>::Iterator i = _cxx_map.first(); i; i++) {
    int t = i.value();
    while (t > 0 && _driver[t] >= 0 && _driver[t] != driver)
      t = i.value() = _cxx_next[t];
  }
}
