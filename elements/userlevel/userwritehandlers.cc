/*
 * userwritehandlers.{cc,hh} -- element runs write handlers as specified by a file
 * Douglas S. J. De Couto
 * inspired by pokehandlers.{cc,hh} by Eddie Kohler
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
#include "userwritehandlers.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include "string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

UserWriteHandlers::UserWriteHandlers() : 
#if !UWH_USE_SELECT
  _timer(timer_hook, (unsigned long) this), _period(0), 
#endif
  _buflen(255 + 1), _pos(0)
{
  _buf = new char[_buflen];
  memset(_buf, 0, _buflen);
}

UserWriteHandlers::~UserWriteHandlers()
{
  uninitialize();
}

int
UserWriteHandlers::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int old_buflen = _buflen;
  int res = cp_va_parse(conf, this, errh,
			cpString, "filename", &_fname,
#if !UWH_USE_SELECT
			cpInteger, "period (msecs)", &_period,
#endif
			cpOptional,
			cpInteger, "max command length", &_buflen,
			0);
#if !UWH_USE_SELECT
  if (_period <= 0)
    return errh->error("%s: period must be greater than 0", id().cc());
#endif
  if (_buflen <= 0)
    return errh->error("%s: The buffer length must be greater than 0", id().cc());
  _buflen += 1; // for the terminal 0

  if (_buflen != old_buflen) {
    delete[] _buf;
    _buf = new char[_buflen];
    memset(_buf, 0, _buflen);
  }
  return res;
}

int
UserWriteHandlers::initialize(ErrorHandler *errh)
{
#if !UWH_USE_SELECT
  _timer.attach(this);
  _timer.schedule_after_ms(_period);
#endif

  _fd = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (_fd < 0) 
    return errh->error("%s: socket: %s", id().cc(), strerror(errno));
  
  struct sockaddr_un uaddr;
  memset(&uaddr, 0, sizeof(uaddr));
  uaddr.sun_family = AF_UNIX;
  if (_fname.length() > (int) sizeof(uaddr.sun_path))
    return errh->error("%s: the unix domain socket pathname `%s' is too long", id().cc(), _fname.cc());
  strncpy(uaddr.sun_path, _fname.cc(), sizeof(uaddr.sun_path));

  unlink(_fname.cc());
  
  int len = _fname.length() + sizeof(uaddr.sun_family);
  int res = bind(_fd, (struct sockaddr *) &uaddr, len);
  if (res < 0)
    return errh->error("%s: bind: %s", id().cc(), strerror(errno));

  res = listen(_fd, 1);
  if (_fd < 1)
    return errh->error("%s: listen: %s", id().cc(), strerror(errno));

  return 0;
}

void
UserWriteHandlers::uninitialize()
{
#if !UWH_USE_SELECT
  _timer.unschedule();
#endif
  close(_fd);
  delete[] _buf; 
}

int
UserWriteHandlers::read_command_strings(String &hndlr_str, String &arg_str, ErrorHandler *errh)
{
  /*
   * this function tries to read some stuff from the socket and stick
   * it into the buffer.  then it reads the next command string in the
   * buffer, even if nothing was actually read.  it doesn't keep
   * reading command strings until there are none to be read, as this
   * could cause click to be starved.  alternative: have the calling
   * function do the read, and then repeatedly call this function to
   * try and find commands to call handlers with.  */

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(_fd, &read_fds);
  
#define DEBUG_SOCK 0
#if DEBUG_SOCK
  click_chatter("before select");
#endif
  struct timeval timeout;
  memset(&timeout, 0, sizeof(timeout));
  int res = select(_fd + 1, &read_fds, 0, 0, &timeout);
  if (res < 0) {
    errh->error("%s: select:", id().cc(), strerror(errno));
    return -1;
  }
  
#if DEBUG_SOCK
  click_chatter("after select");
#endif

  if (res > 0) {
#if DEBUG_SOCK
    click_chatter("trying to recv");
#endif
    res = recv(_fd, _buf + _pos, _buflen - _pos - 1, 0); // -1 to always keep 0 at end
    if (res < 0) {
      errh->error("%s: accept:", id().cc(), strerror(errno));
      return -1;
    }
#if DEBUG_SOCK
    click_chatter("after recv");
#endif
    
#if 0
    // terminate returned pathname string
    len -= sizeof(uaddr.sun_family);
    uaddr.sun_path[len] = 0;
#endif 
  }
  
  _pos += res; // _pos is index of next byte to be read into _buf

  // try to find a command, i.e. if buffer has a \n in it
  int i;
  for (i = 0; _buf[i] != 0 && _buf[i] != '\n'; i++)
    ; // do it

  if (_buf[i] == 0) { 
    // no command found
    if (_pos == _buflen - 1) {
      // is buffer full? THEN clear the buffer (user typed too much!)
      memset(_buf, 0, _buflen);
      _pos = 0;
      errh->warning("%s: handler string buffer overflowed", id().cc());
    }
    return -1;
  }
  
  // pull the handler name and arg out of _buf
  _buf[i] = 0;
  String str(_buf);

  // keep remaining data in buf
  _pos = _pos - i - 1; // calc num bytes leftover in buffer
  memmove(_buf, _buf + i + 1, _pos);
  memset(_buf + _pos, 0, _buflen - _pos);
  
  int hndlr_end = str.find_left(';');
  if (hndlr_end < 0) {
    errh->error("%s: command string `%s' is not of the form `handler-string;arg-string'", 
		id().cc(), str.cc());
    return -1;
  }
  hndlr_str = str.substring(0, hndlr_end);
  arg_str = str.substring(hndlr_end + 1);
  
  return 0;
}

void
UserWriteHandlers::timer_hook(unsigned long thunk)
{
  UserWriteHandlers *uwh = (UserWriteHandlers *) thunk;
  ErrorHandler *errh = ErrorHandler::default_handler();
  Router *router = uwh->router();

#if !UWH_USE_SELECT
  // reschedule
  uwh->_timer.schedule_after_ms(uwh->_period);
#endif

  assert(uwh->_buf[uwh->_buflen - 1] == 0);

  String h_str;
  String arg_str;
  int res = uwh->read_command_strings(h_str, arg_str, errh);
  if (res < 0)
    return;

  // sort handler into element name and handler name
  int i = h_str.find_left('.');
  if (i == h_str.length()) {
    errh->error("%s: handler name `%s' is not in the form element.handler", uwh->id().cc(), h_str.cc());
    return;
  }
    
  String elname;
  elname = h_str.substring(0, i);
  String hname;
  hname = h_str.substring(i + 1);

  Element *he = uwh->router()->find(uwh, elname, errh);
  if (he == 0) {
    errh->error("%s: no element named `%s'", uwh->id().cc(), elname.cc());
    return;
  }
  
  /*
   * call the handler!!! finally...
   */
  i = router->find_handler(he, hname);
  if (i < 0)
    errh->error("%s: no handler `%s.%s'", uwh->id().cc(), he->id().cc(), hname.cc());
  else {
    const Router::Handler &rh = router->handler(i);
    if (rh.write) {
      ContextErrorHandler cerrh(errh, "In write handler `" + he->id() + "." + hname + "':");
      rh.write(arg_str, he, rh.write_thunk, &cerrh);
    } else
      errh->error("%s: no write handler `%s.%s'", uwh->id().cc(), he->id().cc(), hname.cc());
  }

  return;
}

#if UWH_USE_SELECT
void
UserWriteHandlers::selected(int fd)
{
  assert(fd == _fd);
  timer_hook((unsigned long) this);
}
#endif


EXPORT_ELEMENT(UserWriteHandlers)
