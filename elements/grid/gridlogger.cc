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
#include <click/confparse.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include "gridlogger.hh"
CLICK_DECLS

int
GridLogger::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String logfile;
  bool short_ip = true;

  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"LOGFILE", cpString, "logfile name", &logfile,
			"SHORT_IP", cpBool, "log short IP addresses?", &short_ip,
			0);
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

void
GridLogger::add_handlers()
{
  add_default_handlers(false);
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
    return errh->error("unable to open logfile ``%s''", ((String) arg).cc());
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
  int new_fd = open(new_fn.cc(), O_WRONLY | O_CREAT, 0777);
  if (new_fd == -1) {
    click_chatter("GridLogger %s: unable to open log file ``%s'': %s",
		  id().cc(), new_fn.cc(), strerror(errno));
    if (log_is_open())
      click_chatter("Gridlogger %s: previous logging to ``%s'' is still enabled",
		    id().cc(), _fn.cc());
    return false;
  }
  
  if (log_is_open())
    close_log();
  
  _fd = new_fd;
  _fn = new_fn;
  
  click_chatter("GridLogger %s: started logging to %s", id().cc(), _fn.cc());
  return true;
}

void
GridLogger::close_log() {
  if (_fd != -1) {
    close(_fd);
    _fd = -1;
    click_chatter("GridLogger %s: stopped logging on %s", id().cc(), _fn.cc());
    _fn = "";
  }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(GridLogger)
