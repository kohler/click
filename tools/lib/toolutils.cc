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
#include "routert.hh"
#include "lexert.hh"
#include "toolutils.hh"
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
  LexerT lexer(errh);
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
  : _name_map(-1), _cxx_name_map(-1)
{
}

ElementMap::ElementMap(const String &str)
  : _name_map(-1), _cxx_name_map(-1)
{
  parse(str);
}

String
ElementMap::processing_code(const String &n) const
{
  int i = _name_map[n];
  if (i >= 0)
    return _processing_code[i];
  else
    return String();
}

void
ElementMap::add(const String &click_name, String cxx_name,
		String header_file, String processing_code)
{
  if (!click_name)
    return;

  if (cxx_name == "?")
    cxx_name = String();
  if (header_file == "?")
    header_file = String();
  if (processing_code == "?")
    processing_code = String();
  
  int i = _name_map[click_name];
  if (i < 0) {
    i = _name.size();
    _name.push_back(click_name);
    _cxx_name.push_back(cxx_name);
    _header_file.push_back(header_file);
    _processing_code.push_back(processing_code);
    _name_map.insert(click_name, i);
    _cxx_name_map.insert(cxx_name, i);
  } else {
    _name[i] = click_name;
    _cxx_name[i] = cxx_name;
    _header_file[i] = header_file;
    _processing_code[i] = processing_code;
  }
}

void
ElementMap::remove(int i)
{
  if (i < 0 || i >= _name.size())
    return;

  _name_map.insert(_name[i], -1);
  _cxx_name_map.insert(_cxx_name[i], -1);

  if (i < _name.size() - 1) {
    _name[i] = _name.back();
    _cxx_name[i] = _cxx_name.back();
    _header_file[i] = _header_file.back();
    _processing_code[i] = _processing_code.back();
    _name_map.insert(_name[i], i);
    _cxx_name_map.insert(_cxx_name[i], i);
  }
  _name.pop_back();
  _cxx_name.pop_back();
  _header_file.pop_back();
  _processing_code.pop_back();
}

void
ElementMap::parse(const String &str)
{
  Vector<String> a, b, c, d;
  parse_tabbed_lines(str, &a, &b, &c, &d, (void *)0);
  for (int i = 0; i < a.size(); i++)
    add(a[i], b[i], c[i], d[i]);
}

String
ElementMap::unparse() const
{
  StringAccum sa;
  for (int i = 0; i < size(); i++) {
    sa << _name[i] << '\t';
    if (_cxx_name[i])
      sa << _cxx_name[i] << '\t';
    else
      sa << "?\t";
    if (_header_file[i])
      sa << _header_file[i] << '\t';
    else
      sa << "?\t";
    if (_processing_code[i])
      sa << _processing_code[i];
    else
      sa << '?';
    sa << '\n';
  }
  return sa.take_string();
}
