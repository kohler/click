/*
 * common.cc -- common code for click-install and click-uninstall
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2002 International Computer Science Institute
 * Copyright (c) 2006 Regents of the University of California
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

#include <click/glue.hh>
#include "common.hh"
#include "toolutils.hh"
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#if FOR_BSDMODULE
# include <sys/param.h>
# include <sys/mount.h>
#elif FOR_LINUXMODULE && HAVE_CLICKFS
# include <sys/mount.h>
#endif
#include <sys/wait.h>

#if FOR_BSDMODULE || FOR_LINUXMODULE
String clickfs_dir = String::make_stable("/click");
String clickfs_prefix = String::make_stable("/click");
#endif

bool verbose = false;

static void
read_package_string(const String &text, StringMap &packages)
{
  const char *begin = text.begin();
  const char *end = text.end();
  while (begin < end) {
    const char *start = begin;
    while (begin < end && !isspace((unsigned char) *begin))
      begin++;
    packages.set(text.substring(start, begin), 0);
    begin = find(begin, end, '\n') + 1;
  }
}

bool
read_package_file(String filename, StringMap &packages, ErrorHandler *errh)
{
  if (!errh && access(filename.c_str(), F_OK) < 0)
    return false;
  int before = errh->nerrors();
  String str = file_string(filename, errh);
  if (!str && errh->nerrors() != before)
    return false;
  read_package_string(str, packages);
  return true;
}

bool
adjust_clickfs_prefix()
{
#if FOR_LINUXMODULE
    String clickfs_h_prefix = clickfs_prefix + String::make_stable("/.h");
    if (access(clickfs_h_prefix.c_str(), F_OK) >= 0) {
	clickfs_prefix = clickfs_h_prefix;
	return true;
    }
#endif
    return false;
}

bool
read_active_modules(StringMap &packages, ErrorHandler *errh)
{
#if FOR_LINUXMODULE
  return read_package_file("/proc/modules", packages, errh);
#else
  int before = errh->nerrors();
  String output = shell_command_output_string
    ("/sbin/kldstat | /usr/bin/awk \'/Name/ {\n"
     "  for (i = 1; i <= NF; i++)\n"
     "    if ($i == \"Name\")\n"
     "      n = i;\n"
     "  next;\n"
     "}\n"
     "{ print $n }\';", String(), errh);
  if (!output && errh->nerrors() != before)
    return false;
  read_package_string(output, packages);
  return true;
#endif
}

static int
kill_current_configuration(ErrorHandler *errh)
{
  if (verbose)
    errh->message("Installing blank configuration in kernel");
  String clickfs_config = clickfs_prefix + String("/config");
  FILE *f = fopen(clickfs_config.c_str(), "w");
  if (!f)
    errh->fatal("cannot uninstall configuration: %s", strerror(errno));
  fputs("// nothing\n", f);
  fclose(f);
  return 0;
}

int
remove_unneeded_packages(const StringMap &active_modules, const StringMap &packages, ErrorHandler *errh)
{
  // remove extra packages
  Vector<String> removals;

  // go over all modules; figure out which ones are Click packages
  // by checking 'packages' array; mark old Click packages for removal
  for (StringMap::const_iterator iter = active_modules.begin(); iter.live(); iter++)
    // only remove packages that weren't used in this configuration.
    // packages used in this configuration have value > 0
    if (iter.value() == 0) {
      String key = iter.key();
      if (packages[key] >= 0)
	removals.push_back(key);
      else if (key.length() > 3 && key.substring(key.length() - 3) == OBJSUFFIX) {
	// check for .ko/.bo packages
	key = key.substring(0, key.length() - 3);
	if (packages[key] >= 0)
	  removals.push_back(key);
      }
    }

  // actually remove the packages
  int retval = 0;
  if (removals.size()) {
    String to_remove;
    for (int i = 0; i < removals.size(); i++)
	to_remove += (i ? " " : "") + removals[i];
    if (verbose)
      errh->message("Removing packages: %s", to_remove.c_str());

#if FOR_LINUXMODULE
    String cmdline = "/sbin/rmmod " + to_remove + " 2>/dev/null";
    int status = system(cmdline.c_str());
    if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
      retval = errh->error("cannot remove package(s) '%s'", to_remove.c_str());
#elif FOR_BSDMODULE
    for (int i = 0; i < removals.size(); i++) {
      String cmdline = "/sbin/kldunload " + removals[i] + ".bo";
      int status = system(cmdline.c_str());
      if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
	retval = errh->error("cannot remove package '%s'", removals[i].c_str());
    }
#endif
  }
  return retval;
}

int
unload_click(ErrorHandler *errh)
{
  adjust_clickfs_prefix();

  // do nothing if Click not installed
  String clickfs_packages = clickfs_prefix + String("/packages");
  if (access(clickfs_packages.c_str(), F_OK) < 0)
    return 0;

  // first, write nothing to /proc/click/config -- frees up modules
  if (kill_current_configuration(errh) < 0)
    return -1;

  // find current packages
  HashTable<String, int> active_modules(-1);
  HashTable<String, int> packages(-1);
  read_active_modules(active_modules, errh);
  read_package_file(clickfs_packages, packages, errh);

  // remove unused packages
  (void) remove_unneeded_packages(active_modules, packages, errh);

#if FOR_BSDMODULE
  // unmount Click file system
  if (verbose)
    errh->message("Unmounting Click filesystem at %s", clickfs_dir.c_str());
  int unmount_retval = unmount(clickfs_dir.c_str(), MNT_FORCE);
  if (unmount_retval < 0)
    errh->error("cannot unmount %s: %s", clickfs_dir.c_str(), strerror(errno));
#endif

  // remove Click module
  if (verbose)
    errh->message("Removing Click module");
  int status;
#if FOR_LINUXMODULE
  status = system("/sbin/rmmod click");
#elif FOR_BSDMODULE
  status = system("/sbin/kldunload click.ko");
#endif
  if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    return errh->error("cannot remove Click module from kernel");

#if FOR_LINUXMODULE
  // proclikefs will take care of the unmount for us, but we'll give it a shot
  // anyway.
  if (verbose)
    errh->message("Unmounting Click filesystem at %s", clickfs_dir.c_str());
  (void) umount(clickfs_dir.c_str());
#endif

  // see if we successfully removed it
  if (access(clickfs_packages.c_str(), F_OK) >= 0) {
    errh->warning("cannot uninstall Click module");
    return -1;
  }
  return 0;
}
