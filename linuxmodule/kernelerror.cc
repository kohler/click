/*
 * kernelerror.{cc,hh} -- ErrorHandler subclass that saves errors for
 * /proc/click/errors and reports them with printk()
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "modulepriv.hh"

#include "kernelerror.hh"
#include <click/straccum.hh>

static StringAccum *all_errors = 0;


static void
syslog_message(const String &message)
{
  int pos = 0, nl;
  while ((nl = message.find_left('\n', pos)) >= 0) {
    String x = message.substring(pos, nl - pos);
    printk("<1>%s\n", x.cc());
    pos = nl + 1;
  }
  if (pos < message.length()) {
    String x = message.substring(pos);
    printk("<1>%s\n", x.cc());
  }
}


void
KernelErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  if (seriousness <= ERR_MESSAGE)
    /* do nothing */;
  else if (seriousness == ERR_WARNING)
    _nwarnings++;
  else
    _nerrors++;

  syslog_message(message);
  *all_errors << message << "\n";
  
  if (seriousness == ERR_FATAL)
    panic("KernelErrorHandler");
}

void
SyslogErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  syslog_message(message);
}


static String
read_errors(Element *, void *)
{
  if (all_errors)
    // OK to return a stable_string, even though the data is not really
    // stable, because we use it for a very short time (HANDLER_REREAD).
    // Problems are possible, of course.
    return String::stable_string(all_errors->data(), all_errors->length());
  else
    return String::out_of_memory_string();
}

void
reset_proc_click_errors()
{
  all_errors->clear();
}

void
init_proc_click_errors()
{
  all_errors = new StringAccum;
  Router::add_global_read_handler("errors", read_errors, 0);
}

void
cleanup_proc_click_errors()
{
  delete all_errors;
  all_errors = 0;
}
