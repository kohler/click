/*
 * khandlerproxy.{cc,hh} -- element forwards handlers to kernel configuration
 * Eddie Kohler
 *
 * Copyright (c) 2000-2001 Mazu Networks, Inc.
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
#include "khandlerproxy.hh"
#include <click/error.hh>
#include <click/router.hh>
#include <click/confparse.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cctype>
CLICK_DECLS

KernelHandlerProxy::KernelHandlerProxy()
  : _detailed_error_message(false)
{
  MOD_INC_USE_COUNT;
}

KernelHandlerProxy::~KernelHandlerProxy()
{
  MOD_DEC_USE_COUNT;
}

void *
KernelHandlerProxy::cast(const char *n)
{
  if (strcmp(n, "HandlerProxy") == 0)
    return (HandlerProxy *)this;
  else if (strcmp(n, "KernelHandlerProxy") == 0)
    return (Element *)this;
  else
    return 0;
}

int
KernelHandlerProxy::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _verbose = false;
  return cp_va_parse(conf, this, errh,
		     cpKeywords,
		     "VERBOSE", cpBool, "be verbose?", &_verbose,
		     0);
}

void
KernelHandlerProxy::add_handlers()
{
  add_write_handler("*", star_write_handler, 0);
}


static void
complain_to(ErrorHandler *errh, int errcode, const String &complaint)
{
  if (errcode >= 0)
    errh->set_error_code(errcode);
  errh->verror_text(ErrorHandler::ERR_ERROR, String(), complaint);
}

int
KernelHandlerProxy::complain(ErrorHandler *errh, const String &hname,
			     int errcode, const String &complaint)
{
  if (errh)
    complain_to(errh, errcode, complaint);
  else {
    for (int i = 0; i < _nerr_rcvs; i++)
      if ((errh = _err_rcvs[i].hook(hname, _err_rcvs[i].thunk)))
	complain_to(errh, errcode, complaint);
  }
  return -EINVAL;
}

int
KernelHandlerProxy::complain_about_open(ErrorHandler *errh,
					const String &hname, int errno_val)
{
  int dot = hname.find_left('.');
  String k_elt = hname.substring(0, dot);
  
  if (errno_val == ENOENT) {
    String try_fn = "/proc/click/" + k_elt;
    if (access("/proc/click", F_OK) < 0)
      complain(errh, hname, CSERR_NO_ROUTER, "No router installed");
    else if (k_elt != "0" && access(try_fn.cc(), F_OK) < 0)
      complain(errh, hname, CSERR_NO_SUCH_ELEMENT, "No element named `" + k_elt.printable() + "'");
    else
      complain(errh, hname, CSERR_NO_SUCH_HANDLER, "No handler named `" + hname.printable() + "'");
  } else if (errno_val == EACCES)
    complain(errh, hname, CSERR_PERMISSION, "Permission denied for `" + hname.printable() + "'");
  else
    complain(errh, hname, CSERR_UNSPECIFIED, "Handler `" + hname.printable() + "' error: " + String(strerror(errno_val)));

  return -errno_val;
}

int
KernelHandlerProxy::check_handler_name(const String &hname, ErrorHandler *errh)
{
  int dot = hname.find_left('.');
  if (dot <= 0 || dot == hname.length() - 1)
    return complain(errh, hname, CSERR_SYNTAX, "Bad handler name `" + hname.printable() + "'");

  // check characters for validity -- don't want to screw stuff up
  const char *s = hname.data();
  for (int i = 0; i < dot; i++)
    if (!isalnum(s[i]) && s[i] != '_' && s[i] != '/' && s[i] != '@')
      return complain(errh, hname, CSERR_SYNTAX, "Bad character in element name `" + hname.substring(0, dot).printable() + "'");
  for (int i = dot + 1; i < hname.length(); i++)
    if (s[i] < 32 || s[i] >= 127 || s[i] == '/')
      return complain(errh, hname, CSERR_SYNTAX, "Bad character in handler name `" + hname.printable() + "'");

  return 0;
}

static String
handler_name_to_file_name(const String &str)
{
  if (str[0] == '0' && str[1] == '.')
    return "/proc/click/" + str.substring(2);
  else {
    int dot = str.find_left('.');
    return "/proc/click/" + str.substring(0, dot) + "/" + str.substring(dot + 1);
  }
}

int
KernelHandlerProxy::star_write_handler(const String &str, Element *e, void *, ErrorHandler *errh)
{
  KernelHandlerProxy *khp = static_cast<KernelHandlerProxy *>(e);
  if (khp->check_handler_name(str, errh) < 0)
    return -1;
  int which = e->router()->nhandlers();
  e->add_read_handler(str, read_handler, (void *)which);
  e->add_write_handler(str, write_handler, (void *)which);
  assert(e->router()->find_handler(e, str) == which);
  return which;
}

int
KernelHandlerProxy::check_handler(const String &hname, bool write, ErrorHandler *errh)
{
  if (check_handler_name(hname, errh) < 0)
    return 0;

  String fn = handler_name_to_file_name(hname);
  if (access(fn.cc(), (write ? W_OK : R_OK)) < 0) {
    complain_about_open(errh, hname, errno);
    return 0;
  }

  // If accessible, it still might be a directory rather than a handler.
  struct stat buf;
  stat(fn.cc(), &buf);
  if (S_ISDIR(buf.st_mode)) {
    errh->set_error_code(CSERR_NO_SUCH_HANDLER);
    errh->error("No handler named `%#s'", hname.printable().cc());
    return 0;
  } else {
    errh->message("%s handler `%s' OK", (write ? "Write" : "Read"), hname.printable().cc());
    return 1;
  }
}

String
KernelHandlerProxy::read_handler(Element *e, void *thunk)
{
  KernelHandlerProxy *khp = static_cast<KernelHandlerProxy *>(e);
  Router *r = e->router();
  int handleri = (intptr_t)thunk;
  const Router::Handler &h = r->handler(handleri);

  errno = 0;
  String fn = handler_name_to_file_name(h.name());
  String s = file_string(fn, 0);
  int err = errno;

  if (!s && err != 0) {
    if (khp->_verbose)
      khp->complain_about_open(ErrorHandler::default_handler(), h.name(), err);
    // complain to error receivers
    khp->complain_about_open(0, h.name(), err);
  }
  
  return s;
}

int
KernelHandlerProxy::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
  KernelHandlerProxy *khp = static_cast<KernelHandlerProxy *>(e);
  Router *r = e->router();
  int handleri = (intptr_t)thunk;
  const Router::Handler &h = r->handler(handleri);

  String fn = handler_name_to_file_name(h.name());
  int fd = open(fn, O_WRONLY | O_TRUNC);
  
  if (fd < 0)
    return khp->complain_about_open(errh, h.name(), errno);

  int pos = 0;
  const char *data = str.data();
  while (pos < str.length()) {
    int left = str.length() - pos;
    ssize_t written = write(fd, data + pos, left);
    if (written < 0 && errno != EINTR) {
      close(fd);
      return khp->complain(errh, h.name(), CSERR_UNSPECIFIED, fn + ": " + String(strerror(errno)));
    } else if (written >= 0)
      pos += written;
  }

  if (close(fd) < 0) {
    int err = errno;
    khp->complain(errh, h.name(), CSERR_HANDLER_ERROR, "Error executing kernel write handler `" + h.name() + "'");
    if (!khp->_detailed_error_message) {
      khp->complain(errh, h.name(), CSERR_HANDLER_ERROR, "(Check /proc/click/errors for details.)");
      khp->_detailed_error_message = true;
    }
    return -err;
  } else
    return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel HandlerProxy)
EXPORT_ELEMENT(KernelHandlerProxy)
