/*
 * packageutils.cc -- support for packages
 * Eddie Kohler
 *
 * Copyright (c) 2001 Mazu Networks, Inc.
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
#include <click/pathvars.h>

#ifdef CLICK_LINUXMODULE
# define WANT_MOD_USE_COUNT 1	/* glue.hh should not define MOD_USE_COUNTs */
#endif

#include <click/package.hh>
#include <click/hashmap.hh>
#include <click/error.hh>


#if !defined(CLICK_LINUXMODULE) && !defined(CLICK_BSDMODULE)
# include <click/userutils.hh>
# include <unistd.h>
# include <errno.h>
# include <string.h>
# include <stdlib.h>
#endif

#ifdef CLICK_TOOL
# include "lexert.hh"
# include "routert.hh"
#else
# include <click/lexer.hh>
#endif


// GENERIC PACKAGE SUPPORT

struct ClickProvision {
  String name;
  bool loaded : 1;
  int provided;
};
static int nprovisions;
static int provisions_cap;
static ClickProvision *provisions;

static ClickProvision *
find_provision(const String &name)
{
  for (int i = 0; i < nprovisions; i++)
    if (provisions[i].name == name)
      return &provisions[i];

  // otherwise, create new ClickProvision
  ClickProvision *npf;
  if (nprovisions >= provisions_cap) {
    int n = (nprovisions ? nprovisions * 2 : 4);
    if (!(npf = new ClickProvision[n]))
      return 0;
    for (int i = 0; i < nprovisions; i++)
      npf[i] = provisions[i];
    provisions_cap = n;
    delete[] provisions;
    provisions = npf;
  } else
    npf = provisions;
  
  npf[nprovisions].name = name;
  npf[nprovisions].loaded = false;
  npf[nprovisions].provided = 0;
  return &npf[nprovisions++];
}


extern "C" void
click_provide(const char *package)
{
  ClickProvision *p = find_provision(package);
  if (p) {
#ifndef CLICK_TOOL
    MOD_INC_USE_COUNT;
#endif
    p->provided++;
  }
}

extern "C" void
click_unprovide(const char *package)
{
  ClickProvision *p = find_provision(package);
  if (p && p->provided > 0) {
#ifndef CLICK_TOOL
    MOD_DEC_USE_COUNT;
#endif
    p->provided--;
  }
}

extern "C" bool
click_has_provision(const char *package)
{
  ClickProvision *p = find_provision(package);
  return (p && p->provided > 0);
}

extern "C" void
click_public_packages(Vector<String> &v)
{
  for (int i = 0; i < nprovisions; i++)
    if (provisions[i].provided > 0 && provisions[i].name && provisions[i].name[0] != '@')
      v.push_back(provisions[i].name);
}

#ifdef CLICK_LINUXMODULE
extern "C" void
click_cleanup_packages()
{
  delete[] provisions;
  provisions = 0;
  nprovisions = provisions_cap = 0;
}
#endif


#if HAVE_DYNAMIC_LINKING && !defined(CLICK_LINUXMODULE) && !defined(CLICK_BSDMODULE)

static String::Initializer crap_initializer;
static String click_compile_prog, tmpdir;

static int
archive_index(const Vector<ArchiveElement> *archive, const String &what)
{
  if (archive)
    for (int i = 0; i < archive->size(); i++)
      if (archive->at(i).name == what)
	return i;
  return -1;
}

static bool
check_tmpdir(const Vector<ArchiveElement> *archive, ErrorHandler *errh)
{
  // change to temporary directory
  if (tmpdir)
    return tmpdir;
  if (!(tmpdir = click_mktmpdir(errh)))
    return String();

  // find compile program
  if (!click_compile_prog) {
    click_compile_prog = clickpath_find_file("click-compile", "bin", CLICK_BINDIR, errh);
    if (!click_compile_prog)
      return String();
  }

  // store .hh files in temporary directory
  if (archive)
    for (int i = 0; i < archive->size(); i++)
      if ((*archive)[i].name.substring(-3) == ".hh") {
	String filename = tmpdir + (*archive)[i].name;
	FILE *f = fopen(filename, "w");
	if (!f)
	  errh->warning("%s: %s", filename.cc(), strerror(errno));
	else {
	  fwrite((*archive)[i].data.data(), 1, (*archive)[i].data.length(), f);
	  fclose(f);
	}
      }

  return tmpdir;
}

static String
compile_archive_file(String package, const Vector<ArchiveElement> *archive, int ai, ErrorHandler *errh)
{
  if (!check_tmpdir(archive, errh))
    return String();

#ifdef CLICK_TOOL
  String package_file = package + ".to";
  String target = "tool";
#else
  String package_file = package + ".uo";
  String target = "user";
#endif
  
  ContextErrorHandler cerrh
    (errh, "While compiling package `" + package_file + "':");

  // write .cc file
  const ArchiveElement &ae = archive->at(ai);
  String filename = tmpdir + ae.name;
  FILE *f = fopen(filename, "w");
  if (!f) {
    cerrh.error("%s: %s", filename.cc(), strerror(errno));
    return String();
  }
  fwrite(ae.data.data(), 1, ae.data.length(), f);
  fclose(f);
  
  // run click-compile
  String compile_command = click_compile_prog + " --directory=" + tmpdir + " --target=" + target + " --package=" + package_file + " " + filename;
  errh->message("%s", compile_command.cc());
  int compile_retval = system(compile_command.cc());
  if (compile_retval == 127)
    cerrh.error("could not run `%s'", compile_command.cc());
  else if (compile_retval < 0)
    cerrh.error("could not run `%s': %s", compile_command.cc(), strerror(errno));
  else if (compile_retval != 0)
    cerrh.error("`%s' failed", compile_command.cc());
  else
    return tmpdir + package_file;

  return String();
}

void
clickdl_load_requirement(String name, const Vector<ArchiveElement> *archive, ErrorHandler *errh)
{
  ClickProvision *p = find_provision(name);
  if (!p || p->loaded)
    return;

  ContextErrorHandler cerrh(errh, "While loading package `" + name + "':");

#ifdef CLICK_TOOL
  String suffix = ".to", cxx_suffix = ".t.cc";
#else
  String suffix = ".uo", cxx_suffix = ".u.cc";
#endif
  String package;
  
  // check archive
  int ai;
  if ((ai = archive_index(archive, name + suffix)) >= 0) {
    if (!check_tmpdir(archive, &cerrh))
      return;
    package = tmpdir + "/" + name + suffix;
    FILE *f = fopen(package.cc(), "wb");
    if (!f) {
      cerrh.error("cannot open `%s': %s", package.cc(), strerror(errno));
      package = String();
    } else {
      const ArchiveElement &ae = archive->at(ai);
      fwrite(ae.data.data(), 1, ae.data.length(), f);
      fclose(f);
    }
  } else if ((ai = archive_index(archive, name + cxx_suffix)) >= 0)
    package = compile_archive_file(name, archive, ai, &cerrh);
  else if ((ai = archive_index(archive, name + ".cc")) >= 0)
    package = compile_archive_file(name, archive, ai, &cerrh);
  else {
    // search path
    package = clickpath_find_file(name + suffix, "lib", CLICK_LIBDIR);
    if (!package)
      package = clickpath_find_file(name + ".o", "lib", CLICK_LIBDIR);
    if (!package)
      cerrh.error("can't find required package `%s%s'\nin CLICKPATH or `%s'", name.cc(), suffix.cc(), CLICK_LIBDIR);
  }

  p->loaded = true;
  if (package)
    clickdl_load_package(package, &cerrh);
}

#endif /* HAVE_DYNAMIC_LINKING && !defined(CLICK_LINUXMODULE) && !defined(CLICK_BSDMODULE) */
