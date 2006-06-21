// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * umlswitch.{cc,hh} -- connects to uml_switch
 *
 * Mark Huang <mlhuang@cs.princeton.edu>
 * Copyright (C) 2005 The Trustees of Princeton University
 *
 * $Id: umlswitch.cc,v 1.1 2006/06/21 21:07:45 eddietwo Exp $
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "umlswitch.hh"

CLICK_DECLS

int
UMLSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "path to uml_switch control socket", &_ctl_path,
		  cpEnd) < 0)
    return -1;

  return 0;
}

int
UMLSwitch::initialize(ErrorHandler *errh)
{
  int err = 0, addrlen;
  Vector<String> socket_conf;

  // connect to uml_switch control socket
  _control = socket(AF_UNIX, SOCK_STREAM, 0);
  if (_control < 0) {
    err = errh->error("could not create control socket");
    goto done;
  }

  _ctl_addr.sun_family = AF_UNIX;
  if (_ctl_path.length() >= (int)sizeof(((struct sockaddr_un *)0)->sun_path)) {
    err = errh->error("path to uml_switch control socket too long");
    goto done;
  }
  strcpy(_ctl_addr.sun_path, _ctl_path.c_str());
  addrlen = offsetof(struct sockaddr_un, sun_path) + _ctl_path.length() + 1;
  if (connect(_control, (struct sockaddr *)&_ctl_addr, addrlen) < 0) {
    err = errh->error("could not connect to control socket %s: %s", _ctl_path.c_str(), strerror(errno));
    goto done;
  }

  // set up a local address
  struct timeval tv;
  struct {
    char zero;
    int pid;
    int usecs;
  } name;
  name.zero = 0;
  name.pid = getpid();
  gettimeofday(&tv, NULL);
  name.usecs = tv.tv_usec;
  _local_addr.sun_family = AF_UNIX;
  memcpy(_local_addr.sun_path, &name, sizeof(name));

  // tell uml_switch our local address
  struct request_v3 req;
  req.magic = SWITCH_MAGIC;
  req.version = SWITCH_VERSION;
  req.type = REQ_NEW_CONTROL;
  req.sock = _local_addr;
  if (write(_control, &req, sizeof(req)) != sizeof(req)) {
    err = errh->error("control setup request failed: %s", strerror(errno));
    goto done;
  }

  // get remote address from uml_switch
  if (read(_control, &_data_addr, sizeof(_data_addr)) != sizeof(_data_addr)) {
    err = errh->error("read of data socket failed: %s", strerror(errno));
    goto done;
  }

  // configure and initialize Socket parameters
  addrlen = (int)sizeof(((struct sockaddr_un *)0)->sun_path);
  socket_conf.push_back(String("UNIX_DGRAM"));
  socket_conf.push_back(String('"') +
			String::stable_string(_data_addr.sun_path, addrlen).quoted_hex() +
			String('"'));
  socket_conf.push_back(String('"') +
			String::stable_string(_local_addr.sun_path, addrlen).quoted_hex() +
			String('"'));
  socket_conf.push_back(String("CLIENT true"));
  if ((err = Socket::configure(socket_conf, errh)) ||
      (err = Socket::initialize(errh)))
    goto done;

 done:
  return err;
}

void
UMLSwitch::cleanup(CleanupStage stage)
{
  Socket::cleanup(stage);
  if (_control >= 0)
    close(_control);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Socket)
EXPORT_ELEMENT(UMLSwitch)
