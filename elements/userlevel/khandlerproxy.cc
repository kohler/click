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
#include <click/llrpc.h>
#include "elements/userlevel/controlsocket.hh"
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>

KernelHandlerProxy::KernelHandlerProxy()
  : _detailed_error_message(false)
{
  MOD_INC_USE_COUNT;
}

KernelHandlerProxy::~KernelHandlerProxy()
{
  MOD_DEC_USE_COUNT;
}

int
KernelHandlerProxy::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _verbose = false;
  return cp_va_parse(conf, this, errh,
		     cpKeywords,
		     "VERBOSE", cpBool, "be verbose?", &_verbose,
		     0);
}

int
KernelHandlerProxy::local_llrpc(unsigned cmd, void *data)
{
  if (cmd == CLICK_LLRPC_ADD_HANDLER_ERROR_PROXY) {
    ErrorReceiver *nerr_rcvs = new ErrorReceiver[_nerr_rcvs + 1];
    if (!nerr_rcvs) return -ENOMEM;
    memcpy(nerr_rcvs, _err_rcvs, sizeof(ErrorReceiver) * _nerr_rcvs);
    memcpy(nerr_rcvs + _nerr_rcvs, data, sizeof(ErrorReceiver));
    delete[] _err_rcvs;
    _err_rcvs = nerr_rcvs;
    _nerr_rcvs++;
    return 0;
  } else
    return Element::local_llrpc(cmd, data);
}

void
KernelHandlerProxy::add_handlers()
{
  add_write_handler("*", star_write_handler, 0);
}

int
KernelHandlerProxy::star_write_handler(const String &in_str, Element *e, void *, ErrorHandler *errh)
{
  String str = in_str;

  int dot = str.find_left('.');
  if (dot < 0 || dot == str.length() - 1)
    return errh->error("invalid handler name `%#s'", str.cc());

  // check characters for validity -- don't want to screw stuff up
  const char *s = str.data();
  for (int i = 0; i < dot; i++)
    if (!isalnum(s[i]) && s[i] != '_' && s[i] != '/' && s[i] != '@')
      return errh->error("bad character in handler `%#s' element name", str.cc());
  for (int i = dot + 1; i < str.length(); i++)
    if (s[i] < 32 || s[i] >= 127)
      return errh->error("bad character in handler name `%#s'", str.cc());

  int which = e->router()->nhandlers();
  e->add_read_handler(str, read_handler, (void *)which);
  e->add_write_handler(str, write_handler, (void *)which);
  assert(e->router()->find_handler(e, str) == which);
  return which;
}

void
KernelHandlerProxy::complain_to_err_rcvs(int err, const String &k_elt, const String &hname)
{
  int complaint = 0;
  
  if (err == ENOENT) {
    String try_fn = "/proc/click/" + k_elt;
    if (k_elt == "0" || access(try_fn.cc(), F_OK) >= 0)
      complaint = ControlSocket::CSERR_NO_SUCH_HANDLER;
    else
      complaint = ControlSocket::CSERR_NO_SUCH_ELEMENT;
  } else if (err == EACCES)
    complaint = ControlSocket::CSERR_PERMISSION;
  else
    complaint = ControlSocket::CSERR_UNSPECIFIED;

  for (int i = 0; i < _nerr_rcvs; i++)
    _err_rcvs[i].function(hname, complaint, _err_rcvs[i].thunk);
}

String
KernelHandlerProxy::read_handler(Element *e, void *thunk)
{
  KernelHandlerProxy *khp = static_cast<KernelHandlerProxy *>(e);
  Router *r = e->router();
  int handleri = (int)thunk;
  const Router::Handler &h = r->handler(handleri);

  int dot = h.name.find_left('.');
  String k_elt = h.name.substring(0, dot);
  String k_handler = h.name.substring(dot + 1);
  String fn = "/proc/click/";
  if (k_elt != "0")
    fn += k_elt + "/";
  fn += k_handler;

  errno = 0;
  String s = file_string(fn, 0);
  int err = errno;

  if (!s && err != 0) {
    if (khp->_verbose)
      ErrorHandler::default_handler()->error("%s: %s", fn.cc(), strerror(err));
    khp->complain_to_err_rcvs(err, k_elt, h.name);
  }
  
  return s;
}

int
KernelHandlerProxy::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
  KernelHandlerProxy *khp = static_cast<KernelHandlerProxy *>(e);
  Router *r = e->router();
  int handleri = (int)thunk;
  const Router::Handler &h = r->handler(handleri);

  String name = h.name;
  int dot = name.find_left('.');
  String k_elt = name.substring(0, dot);
  String k_handler = name.substring(dot + 1);
  String fn = "/proc/click/";
  if (k_elt != "0")
    fn += k_elt + "/";
  fn += k_handler;

  int fd = open(fn, O_WRONLY | O_TRUNC);
  int err = errno;
  
  if (fd < 0) {
    khp->complain_to_err_rcvs(err, k_elt, h.name);
    if (err == ENOENT)
      return errh->error("no such kernel element `%s'", k_elt.cc());
    else if (err == EACCES)
      return errh->error("%s: permission denied", fn.cc());
    else
      return errh->error("%s: %s", fn.cc(), strerror(err));
  }

  int pos = 0;
  const char *data = str.data();
  while (pos < str.length()) {
    int left = str.length() - pos;
    ssize_t written = write(fd, data + pos, left);
    if (written < 0 && errno != EINTR) {
      close(fd);
      return errh->error("%s: %s", fn.cc(), strerror(errno));
    } else if (written >= 0)
      pos += written;
  }

  err = close(fd);
  if (err < 0) {
    errh->error("error executing kernel write handler %s", name.cc());
    if (!khp->_detailed_error_message) {
      errh->message("(check /proc/click/errors for details)");
      khp->_detailed_error_message = true;
    }
    return -errno;
  } else
    return 0;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(KernelHandlerProxy)
