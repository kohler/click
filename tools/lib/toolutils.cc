/*
 * toolutils.{cc,hh} -- utility routines for tools
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/straccum.hh>
#include <click/bitvector.hh>
#include "routert.hh"
#include "lexert.hh"
#include "toolutils.hh"
#include <click/confparse.hh>
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

  // set readable filename
  if (!filename || strcmp(filename, "-") == 0)
    filename = "<stdin>";

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
      errh->error("%s: archive has no `config' section", filename);
      s = String();
    }
  }

  // read router
  if (!s.length() && !empty_ok)
    errh->warning("%s: empty configuration", filename);
  LexerT lexer(errh, ignore_line_directives);
  lexer.reset(s, filename);
  if (router)
    lexer.set_router(router);
  while (lexer.ystatement())
    /* nada */;
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
  _e.push_back(Elt());
  _def_srcdir.push_back(String());
  _def_compile_flags.push_back(String());
  _def_package.push_back(String());
}

ElementMap::ElementMap(const String &str)
  : _name_map(0), _cxx_map(0)
{
  _e.push_back(Elt());
  _def_srcdir.push_back(String());
  _def_compile_flags.push_back(String());
  _def_package.push_back(String());
  parse(str);
}

const char *
ElementMap::driver_name(int d)
{
  if (d == DRIVER_LINUXMODULE)
    return "kernel";
  else if (d == DRIVER_USERLEVEL)
    return "user-level";
  else
    return "??";
}

const char *
ElementMap::driver_requirement(int d)
{
  if (d == DRIVER_LINUXMODULE)
    return "linuxmodule";
  else if (d == DRIVER_USERLEVEL)
    return "userlevel";
  else
    return "";
}

static bool
requirement_contains(const String &req, const String &n)
{
  int pos = 0;
  while ((pos = req.find_left(n)) >= 0) {
    int rpos = pos + n.length();
    if ((pos == 0 || isspace(req[pos - 1]))
	&& (rpos == req.length() || isspace(req[rpos])))
      return true;
    pos = rpos;
  }
  return false;  
}

bool
ElementMap::requires(int i, const String &n) const
{
  const String &req = _e[i].requirements;
  if (!req)
    return false;
  else
    return requirement_contains(req, n);
}

bool
ElementMap::provides(int i, const String &n) const
{
  if (n == _e[i].name)
    return true;
  
  const String &pro = _e[i].provisions;
  if (!pro)
    return false;
  else
    return requirement_contains(pro, n);
}

const String &
ElementMap::processing_code(const String &n) const
{
  return _e[ _name_map[n] ].processing_code;
}

const String &
ElementMap::flow_code(const String &n) const
{
  return _e[ _name_map[n] ].flow_code;
}

int
ElementMap::flag_value(int ei, int flag) const
{
  const unsigned char *data = reinterpret_cast<const unsigned char *>(_e[ei].flags.data());
  int len = _e[ei].flags.length();
  for (int i = 0; i < len; i++) {
    if (data[i] == flag) {
      if (i < len - 1 && isdigit(data[i+1])) {
	int value = 0;
	for (i++; i < len && isdigit(data[i]); i++)
	  value = 10*value + data[i] - '0';
	return value;
      } else
	return 1;
    } else
      while (i < len && data[i] != ',')
	i++;
  }
  return -1;
}

int
ElementMap::flag_value(const String &n, int flag) const
{
  return flag_value(_name_map[n], flag);
}

const String &
ElementMap::source_directory(int i) const
{
  return _def_srcdir[ _e[i].def_index ];
}

const String &
ElementMap::package(int i) const
{
  return _def_package[ _e[i].def_index ];
}

int
ElementMap::get_driver(const String &requirements)
{
  if (requirement_contains(requirements, "linuxmodule"))
    return DRIVER_LINUXMODULE;
  else if (requirement_contains(requirements, "userlevel"))
    return DRIVER_USERLEVEL;
  else
    return -1;
}

int
ElementMap::add(const Elt &e)
{
  int i = _e.size();
  _e.push_back(e);
  
  Elt &my_e = _e.back();
  if (my_e.requirements)
    my_e.driver = get_driver(my_e.requirements);
  my_e.name_next = _name_map[e.name];
  my_e.cxx_next = _cxx_map[e.name];
  
  if (e.name)
    _name_map.insert(e.name, i);
  if (e.cxx)
    _cxx_map.insert(e.cxx, i);
  
  return i;
}

int
ElementMap::add(const String &click_name, const String &cxx_name,
		const String &header_file, const String &processing_code,
		const String &flow_code, const String &flags,
		const String &requirements, const String &provisions)
{
  Elt e;
  e.name = click_name;
  e.cxx = cxx_name;
  e.header_file = header_file;
  e.processing_code = processing_code;
  e.flow_code = flow_code;
  e.flags = flags;
  e.requirements = requirements;
  e.provisions = provisions;
  return add(e);
}

int
ElementMap::add(const String &click_name, const String &cxx_name,
		const String &header_file, const String &processing_code,
		const String &flow_code)
{
  return add(click_name, cxx_name, header_file, processing_code, flow_code,
	     String(), String(), String());
}

void
ElementMap::remove(int i)
{
  if (i <= 0 || i >= _e.size())
    return;

  Elt &e = _e[i];
  int p = -1;
  for (int t = _name_map[e.name]; t > 0; p = t, t = _e[t].name_next)
    /* nada */;
  if (p >= 0)
    _e[p].name_next = e.name_next;
  else
    _name_map.insert(e.name, e.name_next);

  p = -1;
  for (int t = _cxx_map[e.cxx]; t > 0; p = t, t = _e[t].cxx_next)
    /* nada */;
  if (p >= 0)
    _e[p].cxx_next = e.cxx_next;
  else
    _cxx_map.insert(e.cxx, e.cxx_next);

  e.name = String();
  e.cxx = String();
}

void
ElementMap::parse(const String &str, const String &package_name)
{
  int p, len = str.length();
  int endp = 0;

  int def_index = 0;
  if (package_name != _def_package[0]) {
    def_index = _def_srcdir.size();
    _def_srcdir.push_back(String());
    _def_compile_flags.push_back(String());
    _def_package.push_back(package_name);
  }
  
  for (p = 0; p < len; p = endp + 1) {
    // read a line
    endp = str.find_left('\n', p);
    if (endp < 0)
      endp = str.length();
    String line = str.substring(p, endp - p);

    // break into words
    Vector<String> words;
    cp_spacevec(line, words);

    // skip blank lines & comments
    if (words.size() == 0 || words[0][0] == '#')
      continue;

    // check for $sourcedir
    if (words[0] == "$sourcedir") {
      if (words.size() == 2) {
	def_index = _def_srcdir.size();
	_def_srcdir.push_back(cp_unquote(words[1]));
	_def_compile_flags.push_back(_def_compile_flags[def_index - 1]);
	_def_package.push_back(package_name);
      }

    } else if (words[0][0] != '$' && words.size() >= 4) {
      // an actual line
      Elt elt;
      elt.name = cp_unquote(words[0]);
      elt.cxx = cp_unquote(words[1]);
      elt.header_file = cp_unquote(words[2]);
      elt.processing_code = cp_unquote(words[3]);
      if (words.size() >= 5)
	elt.flow_code = cp_unquote(words[4]);
      if (words.size() >= 6)
	elt.flags = cp_unquote(words[5]);
      if (words.size() >= 7)
	elt.requirements = cp_unquote(words[6]);
      if (words.size() >= 8)
	elt.provisions = cp_unquote(words[7]);
      elt.def_index = def_index;
      (void) add(elt);
    }
  }
}

void
ElementMap::parse(const String &str)
{
  parse(str, String());
}

String
ElementMap::unparse() const
{
  StringAccum sa;
  for (int i = 1; i < _e.size(); i++) {
    const Elt &e = _e[i];
    if (!e.name && !e.cxx)
      continue;
    sa << cp_quote(e.name) << '\t'
       << cp_quote(e.cxx) << '\t'
       << cp_quote(e.header_file) << '\t'
       << cp_quote(e.processing_code) << '\t'
       << cp_quote(e.flow_code) << '\t'
       << cp_quote(e.flags) << '\t'
       << cp_quote(e.requirements) << '\t'
       << cp_quote(e.provisions) << '\n';
  }
  return sa.take_string();
}

void
ElementMap::map_indexes(const RouterT *r, Vector<int> &map_indexes,
			ErrorHandler *errh = 0) const
{
  assert(r->is_flat());
  map_indexes.assign(r->ntypes(), -1);
  for (int i = 0; i < r->nelements(); i++) {
    int t = r->etype(i);
    if (t >= 0 && map_indexes[t] == -1) {
      int idx = _name_map[r->type_name(t)];
      if (idx <= 0) {
	if (errh)
	  errh->lerror(r->elandmark(i), "unknown element class `%s'", String(r->type_name(t)).cc());
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
    if (idx > 0 && _e[idx].driver >= 0)
      return false;
  }
  return true;
}

bool
ElementMap::driver_compatible(const Vector<int> &map_indexes, int driver) const
{
  for (int i = 0; i < map_indexes.size(); i++) {
    int idx = map_indexes[i];
    if (idx <= 0 || _e[idx].driver < 0)
      continue;
    bool any = false;
    while (idx > 0 && !any) {
      if (_e[idx].driver < 0 || _e[idx].driver == driver)
	any = true;
      else
	idx = _e[idx].name_next;
    }
    if (!any)
      return false;
  }
  return true;
}

bool
ElementMap::driver_compatible(const RouterT *router, int driver, ErrorHandler *errh = 0) const
{
  Vector<int> map;
  map_indexes(router, map, errh);
  return driver_compatible(map, driver);
}

void
ElementMap::limit_driver(int driver)
{
  for (HashMap<String, int>::Iterator i = _name_map.first(); i; i++) {
    int t = i.value();
    while (t > 0 && _e[t].driver >= 0 && _e[t].driver != driver)
      t = i.value() = _e[t].name_next;
  }
  for (HashMap<String, int>::Iterator i = _cxx_map.first(); i; i++) {
    int t = i.value();
    while (t > 0 && _e[t].driver >= 0 && _e[t].driver != driver)
      t = i.value() = _e[t].cxx_next;
  }
}


bool
ElementMap::parse_default_file(const String &default_path, ErrorHandler *errh)
{
  String default_fn = clickpath_find_file("elementmap", "share/click", default_path);
  if (default_fn) {
    String text = file_string(default_fn, errh);
    parse(text);
    return true;
  } else {
    errh->warning("cannot find default elementmap");
    return false;
  }
}

bool
ElementMap::parse_requirement_files(RouterT *r, const String &default_path, ErrorHandler *errh, String *not_found_store = 0)
{
  String not_found;

  // try elementmap in archive
  int defaultmap_aei = r->archive_index("elementmap");
  if (defaultmap_aei >= 0)
    parse(r->archive(defaultmap_aei).data, "<archive>");
  
  // parse elementmaps for requirements in required order
  const Vector<String> &requirements = r->requirements();
  for (int i = 0; i < requirements.size(); i++) {
    String req = requirements[i];
    String mapname = "elementmap." + req;

    // look for elementmap in archive
    int map_aei = r->archive_index(mapname);
    if (map_aei >= 0)
      parse(r->archive(map_aei).data, req);
    else {
      String fn = clickpath_find_file(mapname, "share/click", default_path);
      if (fn) {
	String text = file_string(fn, errh);
	parse(text, req);
      } else {
	if (not_found)
	  not_found += ", ";
	not_found += "`" + req + "'";
      }
    }
  }

  if (not_found_store)
    *not_found_store = not_found;
  if (not_found) {
    errh->warning("cannot find package-specific elementmaps:\n  %s", not_found.cc());
    return false;
  } else
    return true;
}

bool
ElementMap::parse_all_files(RouterT *r, const String &default_path, ErrorHandler *errh)
{
  bool found_default = parse_default_file(default_path, errh);
  bool found_other = parse_requirement_files(r, default_path, errh);

  if (found_default && found_other)
    return true;
  else {
    report_file_not_found(default_path, found_default, errh);
    return false;
  }
}

void
ElementMap::report_file_not_found(String default_path, bool found_default,
				  ErrorHandler *errh)
{
  if (!found_default)
    errh->message("(You may get unknown element class errors.\nTry `make install' or set the CLICKPATH evironment variable.");
  else
    errh->message("(You may get unknown element class errors.");

  const char *path = clickpath();
  bool allows_default = path_allows_default_path(path);
  if (!allows_default)
    errh->message("Searched in CLICKPATH `%s'.)", path);
  else if (!path)
    errh->message("Searched in install directory `%s'.)", default_path.cc());
  else
    errh->message("Searched in CLICKPATH and `%s'.)", default_path.cc());
}


// template instance
#include <click/vector.cc>
template class Vector<ElementMap::Elt>;
