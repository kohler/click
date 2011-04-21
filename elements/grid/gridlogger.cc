/*
 * gridlogger.cc
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include "gridlogger.hh"
CLICK_DECLS

GridLogger::GridLogger()
  : GridGenericLogger(), _state(WAITING), _fd(-1), _bufptr(0)
{
}

GridLogger::~GridLogger() {
  if (log_is_open())
    close_log();
}

int
GridLogger::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String logfile;
  bool short_ip = true;

  int res = Args(conf, this, errh)
      .read("LOGFILE", logfile)
      .read("SHORT_IP", short_ip)
      .complete();
  if (res < 0)
    return res;

  _log_full_ip = !short_ip;
  if (logfile.length() > 0) {
    bool ok = open_log(logfile);
    if (!ok)
      return -1;
  }

  return 0;
}

void *
GridLogger::cast(const char *n)
{
  if (strcmp(n, "GridLogger") == 0)
    return (GridLogger *) this;
  else if (strcmp(n, "GridGenericLogger") == 0)
    return (GridGenericLogger *) this;
  else
    return 0;
}


void
GridLogger::add_handlers()
{
  add_read_handler("logfile", read_logfile, 0);
  add_write_handler("start_log", write_start_log, 0);
  add_write_handler("stop_log", write_stop_log, 0);
}

String
GridLogger::read_logfile(Element *e, void *)
{
  GridLogger *g = (GridLogger *) e;
  if (g->log_is_open())
    return g->_fn + "\n";

  return "\n";
}

int
GridLogger::write_start_log(const String &arg, Element *e,
			    void *, ErrorHandler *errh)
{
  GridLogger *g = (GridLogger *) e;
  if (g->log_is_open())
    g->close_log();
  bool res = g->open_log(arg);
  if (!res)
    return errh->error("unable to open logfile ``%s''", ((String) arg).c_str());
  return 0;
}

int
GridLogger::write_stop_log(const String &, Element *e,
			    void *, ErrorHandler *)
{
  GridLogger *g = (GridLogger *) e;
  if (g->log_is_open())
    g->close_log();
  return 0;
}

bool
GridLogger::open_log(const String &filename)
{
  String new_fn = filename;
  int new_fd = open(new_fn.c_str(), O_WRONLY | O_CREAT, 0777);
  if (new_fd == -1) {
    click_chatter("GridLogger %s: unable to open log file ``%s'': %s",
		  name().c_str(), new_fn.c_str(), strerror(errno));
    if (log_is_open())
      click_chatter("Gridlogger %s: previous logging to ``%s'' is still enabled",
		    name().c_str(), _fn.c_str());
    return false;
  }

  if (log_is_open())
    close_log();

  _fd = new_fd;
  _fn = new_fn;

  click_chatter("GridLogger %s: started logging to %s", name().c_str(), _fn.c_str());
  return true;
}

void
GridLogger::close_log() {
  if (_fd != -1) {
    close(_fd);
    _fd = -1;
    click_chatter("GridLogger %s: stopped logging on %s", name().c_str(), _fn.c_str());
    _fn = "";
  }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(GridGenericLogger)
EXPORT_ELEMENT(GridLogger)
