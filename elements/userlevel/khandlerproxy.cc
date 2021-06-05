/*
 * khandlerproxy.{cc,hh} -- element forwards handlers to kernel configuration
 * Eddie Kohler
 *
 * Copyright (c) 2000-2001 Mazu Networks, Inc.
 * Copyright (c) 2008 Meraki, Inc.
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
#include <click/args.hh>
#include <click/userutils.hh>
#include <click/llrpc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
CLICK_DECLS

KernelHandlerProxy::KernelHandlerProxy()
    : _detailed_error_message(false), _dot_h_checked(false), _dot_h(false)
{
}

KernelHandlerProxy::~KernelHandlerProxy()
{
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
    return Args(conf, this, errh).read("VERBOSE", _verbose).complete();
}

void
KernelHandlerProxy::add_handlers()
{
  add_write_handler("*", star_write_handler, 0);
}


static void
complain_to(ErrorHandler *errh, int errcode, const String &complaint)
{
    String csc;
    if (errcode >= 0)
	csc = ErrorHandler::make_anno("cserr", String(errcode));
    errh->xmessage(ErrorHandler::e_error + csc, complaint);
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
  const char *dot = find(hname, '.');
  String k_elt = hname.substring(hname.begin(), dot);

  if (errno_val == ENOENT) {
    String try_fn = "/click/" + k_elt;
    if (access("/click", F_OK) < 0)
      complain(errh, hname, CSERR_NO_ROUTER, "No router installed");
    else if (k_elt != "" && access(try_fn.c_str(), F_OK) < 0)
      complain(errh, hname, CSERR_NO_SUCH_ELEMENT, "No element named '" + k_elt.printable() + "'");
    else
      complain(errh, hname, CSERR_NO_SUCH_HANDLER, "No handler named '" + hname.printable() + "'");
  } else if (errno_val == EACCES)
    complain(errh, hname, CSERR_PERMISSION, "Permission denied for '" + hname.printable() + "'");
  else
    complain(errh, hname, CSERR_UNSPECIFIED, "Handler '" + hname.printable() + "' error: " + String(strerror(errno_val)));

  return -errno_val;
}

int
KernelHandlerProxy::check_handler_name(const String &hname, ErrorHandler *errh)
{
  const char *dot = find(hname, '.');
  if (dot >= hname.end() - 1)
    return complain(errh, hname, CSERR_SYNTAX, "Bad handler name '" + hname.printable() + "'");

  // check characters for validity -- don't want to screw stuff up
  for (const char *s = hname.begin(); s < dot; s++)
    if (!isalnum((unsigned char) *s) && *s != '_' && *s != '/' && *s != '@')
      return complain(errh, hname, CSERR_SYNTAX, "Bad character in element name '" + hname.substring(hname.begin(), dot).printable() + "'");
  for (const char *s = dot + 1; s < hname.end(); s++)
    if (*s < 32 || *s >= 127 || *s == '/')
      return complain(errh, hname, CSERR_SYNTAX, "Bad character in handler name '" + hname.printable() + "'");
  if ((dot == hname.end() - 2 && dot[1] == '.')
      || (dot == hname.end() - 3 && dot[1] == '.' && dot[2] == '.'))
      return complain(errh, hname, CSERR_SYNTAX, "Bad handler name '" + hname.printable() + "'");

  return 0;
}

String
KernelHandlerProxy::handler_name_to_file_name(const String &str)
{
    if (!_dot_h_checked) {
	_dot_h_checked = true;
	_dot_h = (access("/click/.h", F_OK) >= 0);
    }

    const char *dot = find(str, '.');
    String hpart = str.substring(dot + 1, str.end());
    if (_dot_h)
	hpart = String::make_stable(".h/", 3) + hpart;

    if (dot == str.begin())
	return "/click/" + hpart;
    else
	return "/click/" + str.substring(str.begin(), dot) + "/" + hpart;
}

int
KernelHandlerProxy::star_write_handler(const String &str, Element *e, void *, ErrorHandler *errh)
{
    KernelHandlerProxy *khp = static_cast<KernelHandlerProxy *>(e);
    if (khp->check_handler_name(str, errh) < 0)
	return -1;
    khp->set_handler(str, Handler::OP_READ | Handler::OP_WRITE, handler_hook);
    return Router::hindex(e, str);
}

int
KernelHandlerProxy::check_handler(const String &hname, bool write, ErrorHandler *errh)
{
  if (check_handler_name(hname, errh) < 0)
    return 0;

  String fn = handler_name_to_file_name(hname);
  if (access(fn.c_str(), (write ? W_OK : R_OK)) < 0) {
    complain_about_open(errh, hname, errno);
    return 0;
  }

  // If accessible, it still might be a directory rather than a handler.
  struct stat buf;
  stat(fn.c_str(), &buf);
  if (S_ISDIR(buf.st_mode)) {
    errh->error("{cserr:%d}No handler named '%#s'", CSERR_NO_SUCH_HANDLER, hname.printable().c_str());
    return 0;
  } else {
    errh->message("%s handler '%s' OK", (write ? "Write" : "Read"), hname.printable().c_str());
    return 1;
  }
}

int
KernelHandlerProxy::handler_hook(int op, String& str, Element* e, const Handler* handler, ErrorHandler* errh)
{
    KernelHandlerProxy *khp = static_cast<KernelHandlerProxy *>(e);
    const String& hname = handler->name();
    String fn = khp->handler_name_to_file_name(hname);

    if (op == Handler::OP_READ) {
	errno = 0;
	str = file_string(fn, 0);
	int err = errno;
	if (!str && err != 0) {
	    if (khp->_verbose)
		khp->complain_about_open(ErrorHandler::default_handler(), hname, err);
	    // complain to error receivers
	    khp->complain_about_open(0, hname, err);
	}
	return -err;

    } else if (op == Handler::OP_WRITE) {
	int fd = open(fn.c_str(), O_WRONLY | O_TRUNC);
	if (fd < 0)
	    return khp->complain_about_open(errh, hname, errno);

	const char* s = str.begin();
	const char* end = str.end();
	while (s < end) {
	    ssize_t written = write(fd, s, end - s);
	    if (written < 0 && errno != EINTR) {
		close(fd);
		return khp->complain(errh, hname, CSERR_UNSPECIFIED, fn + ": " + String(strerror(errno)));
	    } else if (written >= 0)
		s += written;
	}

	if (close(fd) < 0) {
	    int err = errno;
	    khp->complain(errh, hname, CSERR_HANDLER_ERROR, "Error executing kernel write handler '" + String(hname) + "'");
	    if (!khp->_detailed_error_message) {
		khp->complain(errh, hname, CSERR_HANDLER_ERROR, "(Check /click/errors for details.)");
		khp->_detailed_error_message = true;
	    }
	    return -err;
	} else
	    return 0;

    } else
	return errh->error("odd operation");
}

int
KernelHandlerProxy::llrpc(unsigned command, void *data)
{
  if (command == CLICK_LLRPC_PROXY) {
    click_llrpc_proxy_st* proxy = static_cast<click_llrpc_proxy_st*>(data);
    const Handler* h = static_cast<Handler*>(proxy->proxied_handler);

    String fn = handler_name_to_file_name(h->name());
    int fd = open(fn.c_str(), O_RDONLY);
    int err = errno;

    if (fd < 0) {
      if (_verbose)
	complain_about_open(ErrorHandler::default_handler(), h->name(), err);
      // complain to error receivers and return
      return complain_about_open(0, h->name(), err);
    }

    int retval = ioctl(fd, proxy->proxied_command, proxy->proxied_data);
    err = errno;

    close(fd);
    return (retval >= 0 ? retval : -err);

  } else
    return Element::llrpc(command, data);
}

ELEMENT_REQUIRES(userlevel HandlerProxy)
EXPORT_ELEMENT(KernelHandlerProxy)
CLICK_ENDDECLS
