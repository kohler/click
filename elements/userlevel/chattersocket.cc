/*
 * chattersocket.{cc,hh} -- element echoes chatter to TCP/IP or Unix-domain
 * sockets
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding. */

#include <click/config.h>
#include "chattersocket.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <clicknet/tcp.h>	/* for SEQ_LT, etc. */
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>
CLICK_DECLS

const char ChatterSocket::protocol_version[] = "1.0";

struct ChatterSocketErrorHandler : public ErrorVeneer {

    Vector<ChatterSocket *> _chatter_sockets;

  public:

    ChatterSocketErrorHandler(ErrorHandler *errh)
	: ErrorVeneer(errh) {
    }

    int nchatter_sockets() const	{ return _chatter_sockets.size(); }

    void add_chatter_socket(ChatterSocket *);
    void remove_chatter_socket(ChatterSocket *);

    void *emit(const String &str, void *user_data, bool more);

};

void
ChatterSocketErrorHandler::add_chatter_socket(ChatterSocket *cs)
{
  for (int i = 0; i < _chatter_sockets.size(); i++)
    if (_chatter_sockets[i] == cs)
      return;
  _chatter_sockets.push_back(cs);
}

void
ChatterSocketErrorHandler::remove_chatter_socket(ChatterSocket *cs)
{
  for (int i = 0; i < _chatter_sockets.size(); i++)
    if (_chatter_sockets[i] == cs) {
      _chatter_sockets[i] = _chatter_sockets.back();
      _chatter_sockets.pop_back();
      return;
    }
}

void *
ChatterSocketErrorHandler::emit(const String &str, void *user_data, bool more)
{
    user_data = ErrorVeneer::emit(str, user_data, more);

    String landmark;
    const char *s = parse_anno(str, str.begin(), str.end(),
			       "l", &landmark, (const char *) 0);
    String x = clean_landmark(landmark, true) + str.substring(s, str.end())
	+ String("\n");

    for (int i = 0; i < _chatter_sockets.size(); i++)
	_chatter_sockets[i]->emit(x);
    return user_data;
}


static ChatterSocketErrorHandler *chatter_socket_errh;
static ErrorHandler *base_default_errh;

ChatterSocket::ChatterSocket()
  : _socket_fd(-1), _channel("default"), _retry_timer(0)
{
}

ChatterSocket::~ChatterSocket()
{
}

int
ChatterSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String socktype;
    Args args = Args(this, errh).bind(conf);
    if (args.read_mp("TYPE", socktype).execute() < 0)
	return -1;

    // remove keyword arguments
    bool quiet_channel = true, greeting = true, retry_warnings = true;
    _retries = 0;
    if (args.read("CHANNEL", WordArg(), _channel)
	.read("QUIET_CHANNEL", quiet_channel)
	.read("GREETING", greeting)
	.read("RETRIES", _retries)
	.read("RETRY_WARNINGS", retry_warnings)
	.consume() < 0)
	return -1;
    _greeting = greeting;
    _retry_warnings = retry_warnings;

    socktype = socktype.upper();
    if (socktype == "TCP") {
	_tcp_socket = true;
	uint16_t portno;
	if (args.read_mp("PORT", IPPortArg(IP_PROTO_TCP), portno)
	    .complete() < 0)
	    return -1;
	_unix_pathname = String(portno);

    } else if (socktype == "UNIX") {
	_tcp_socket = false;
	if (args.read_mp("FILENAME", FilenameArg(), _unix_pathname)
	    .complete() < 0)
	    return -1;
	if (_unix_pathname.length() >= (int)sizeof(((struct sockaddr_un *)0)->sun_path))
	    return errh->error("filename too long");

    } else
	return errh->error("unknown socket type `%s'", socktype.c_str());

  // Create channel now, so that other configure() methods will get it.
  ChatterSocketErrorHandler *cserrh;
  if (_channel == "default" && chatter_socket_errh)
    cserrh = chatter_socket_errh;
  else if (_channel == "default") {
    base_default_errh = ErrorHandler::default_handler();
    chatter_socket_errh = new ChatterSocketErrorHandler(base_default_errh);
    ErrorHandler::set_default_handler(chatter_socket_errh);
    cserrh = chatter_socket_errh;
  } else if (void *v = router()->attachment("ChatterChannel." + _channel))
    cserrh = (ChatterSocketErrorHandler *)v;
  else {
    ErrorHandler *base = (quiet_channel ? ErrorHandler::silent_handler() : base_default_errh);
    if (!base) base = ErrorHandler::default_handler();
    cserrh = new ChatterSocketErrorHandler(base);
    router()->set_attachment("ChatterChannel." + _channel, cserrh);
  }

  // install ChatterSocketErrorHandler
  cserrh->add_chatter_socket(this);

  return 0;
}


int
ChatterSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;		// preserve errno

  if (_socket_fd >= 0) {
    close(_socket_fd);
    _socket_fd = -1;
  }

  if (_retries >= 0) {
    if (_retry_warnings)
      errh->warning("%s: %s (%d %s left)", syscall, strerror(e), _retries + 1, (_retries == 0 ? "try" : "tries"));
    return -EINVAL;
  } else
    return errh->error("%s: %s", syscall, strerror(e));
}

int
ChatterSocket::initialize_socket(ErrorHandler *errh)
{
  _retries--;

  // open socket, set options, bind to address
  if (_tcp_socket) {
    _socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return initialize_socket_error(errh, "socket");
    int sockopt = 1;
    if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockopt, sizeof(sockopt)) < 0)
      errh->warning("setsockopt: %s", strerror(errno));

    // bind to port
    int portno = -1;
    (void) IntArg().parse(_unix_pathname, portno);
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portno);
    sa.sin_addr = IPAddress().in_addr();
    if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
      return initialize_socket_error(errh, "bind");

  } else {
    _socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (_socket_fd < 0)
      return initialize_socket_error(errh, "socket");

    // bind to port
    struct sockaddr_un sa;
    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path, _unix_pathname.c_str(), _unix_pathname.length() + 1);
    if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
      return initialize_socket_error(errh, "bind");
  }

  // start listening
  if (listen(_socket_fd, 2) < 0)
    return initialize_socket_error(errh, "listen");

  // nonblocking I/O and close-on-exec for the socket
  fcntl(_socket_fd, F_SETFL, O_NONBLOCK);
  fcntl(_socket_fd, F_SETFD, FD_CLOEXEC);

  add_select(_socket_fd, SELECT_READ);
  return 0;
}

void
ChatterSocket::retry_hook(Timer *t, void *thunk)
{
  ChatterSocket *cs = (ChatterSocket *)thunk;
  if (cs->_socket_fd >= 0)
    /* nada */;
  else if (cs->initialize_socket(ErrorHandler::default_handler()) >= 0)
    /* nada */;
  else if (cs->_retries >= 0)
    t->reschedule_after_sec(1);
  else
    cs->router()->please_stop_driver();
}

int
ChatterSocket::initialize(ErrorHandler *errh)
{
  _max_pos = 0;
  _live_fds = 0;

  if (initialize_socket(errh) >= 0)
    return 0;
  else if (_retries >= 0) {
    _retry_timer = new Timer(retry_hook, this);
    _retry_timer->initialize(this);
    _retry_timer->schedule_after_sec(1);
    return 0;
  } else
    return -1;
}

void
ChatterSocket::take_state(Element *e, ErrorHandler *errh)
{
  ChatterSocket *cs = (ChatterSocket *)e->cast("ChatterSocket");
  if (!cs)
    return;

  if (_socket_fd >= 0) {
    errh->error("already initialized, can't take state");
    return;
  } else if (_tcp_socket != cs->_tcp_socket
	     || _unix_pathname != cs->_unix_pathname
	     || _channel != cs->_channel) {
    errh->error("incompatible ChatterSockets");
    return;
  }

  _socket_fd = cs->_socket_fd;
  cs->_socket_fd = -1;
  _messages.swap(cs->_messages);
  _message_pos.swap(cs->_message_pos);
  _max_pos = cs->_max_pos;
  _fd_alive.swap(cs->_fd_alive);
  _fd_pos.swap(cs->_fd_pos);
  _live_fds = cs->_live_fds;
  cs->_live_fds = 0;

  if (_socket_fd >= 0)
    add_select(_socket_fd, SELECT_READ);
  for (int i = 0; i < _fd_alive.size(); i++)
    if (_fd_alive[i])
      add_select(i, SELECT_WRITE);
}

static void
remove_chatter_channel(ChatterSocketErrorHandler *&cserrh, ChatterSocket *cs)
{
  if (cserrh) {
    cserrh->remove_chatter_socket(cs);
    if (!cserrh->nchatter_sockets()) {
      if (cserrh == chatter_socket_errh)
	ErrorHandler::set_default_handler(base_default_errh);
      delete cserrh;
      cserrh = 0;
    }
  }
}

void
ChatterSocket::cleanup(CleanupStage)
{
  if (_socket_fd >= 0) {
    // shut down the listening socket in case we forked
#ifdef SHUT_RDWR
    shutdown(_socket_fd, SHUT_RDWR);
#else
    shutdown(_socket_fd, 2);
#endif
    close(_socket_fd);
    if (!_tcp_socket)
      unlink(_unix_pathname.c_str());
    _socket_fd = -1;
  }

  for (int i = 0; i < _fd_alive.size(); i++)
    if (_fd_alive[i]) {
      close(i);
      _fd_alive[i] = 0;
    }
  _live_fds = 0;

  if (_retry_timer) {
    delete _retry_timer;
    _retry_timer = 0;
  }

  // unhook from chatter socket error handler
  if (_channel == "default")
    remove_chatter_channel(chatter_socket_errh, this);
  else
    remove_chatter_channel
      ((ChatterSocketErrorHandler *&)(router()->force_attachment("ChatterChannel." + _channel)), this);
}

int
ChatterSocket::flush(int fd)
{
  // check file descriptor
  if (fd >= _fd_alive.size() || !_fd_alive[fd])
    return _messages.size();

  // check if all data written
  if (_fd_pos[fd] == _max_pos)
    return _messages.size();

  // find first useful message (binary search)
  uint32_t fd_pos = _fd_pos[fd];
  int l = 0, r = _messages.size() - 1, useful_message = -1;
  while (l <= r) {
    int m = (l + r) >> 1;
    if (SEQ_LT(fd_pos, _message_pos[m]))
      r = m - 1;
    else if (SEQ_GEQ(fd_pos, _message_pos[m] + _messages[m].length()))
      l = m + 1;
    else {
      useful_message = m;
      break;
    }
  }

  // if messages found, write data until blocked or closed
  if (useful_message >= 0) {
    while (useful_message < _message_pos.size()) {
      const String &m = _messages[useful_message];
      int mpos = _message_pos[useful_message];
      const char *data = m.data() + (fd_pos - mpos);
      int len = m.length() - (fd_pos - mpos);
      int w = write(fd, data, len);
      if (w < 0 && errno != EINTR) {
	if (errno != EAGAIN)	// drop connection on error, except WOULDBLOCK
	  useful_message = -1;
	break;
      } else if (w > 0)
	fd_pos += w;
      if (SEQ_GEQ(fd_pos, mpos + m.length()))
	useful_message++;
    }
  }

  // store changed fd_pos
  _fd_pos[fd] = fd_pos;

  // close out on error, or if socket falls too far behind
  if (useful_message < 0 || SEQ_LT(fd_pos, _max_pos - MAX_BACKLOG)) {
    close(fd);
    remove_select(fd, SELECT_WRITE);
    _fd_alive[fd] = 0;
    _live_fds--;
  } else if (fd_pos == _max_pos)
    remove_select(fd, SELECT_WRITE);
  else
    add_select(fd, SELECT_WRITE);

  return useful_message;
}

void
ChatterSocket::flush()
{
  int min_useful_message = _messages.size();
  if (min_useful_message)
    for (int i = 0; i < _fd_alive.size(); i++)
      if (_fd_alive[i] >= 0) {
	int m = flush(i);
	if (m < min_useful_message)
	  min_useful_message = m;
      }

  // cull old messages
  if (min_useful_message >= 10) {
    _messages.erase(_messages.begin(), _messages.begin() + min_useful_message);
    _message_pos.erase(_message_pos.begin(), _message_pos.begin() + min_useful_message);
  }
}

void
ChatterSocket::selected(int fd, int)
{
  if (fd == _socket_fd) {
    union { struct sockaddr_in in; struct sockaddr_un un; } sa;
#if HAVE_ACCEPT_SOCKLEN_T
    socklen_t sa_len;
#else
    int sa_len;
#endif
    sa_len = sizeof(sa);
    int new_fd = accept(_socket_fd, (struct sockaddr *)&sa, &sa_len);

    if (new_fd < 0) {
      if (errno != EAGAIN)
	click_chatter("%s: accept: %s", declaration().c_str(), strerror(errno));
      return;
    }

    fcntl(new_fd, F_SETFL, O_NONBLOCK);
    fcntl(new_fd, F_SETFD, FD_CLOEXEC);

    while (new_fd >= _fd_alive.size()) {
      _fd_alive.push_back(0);
      _fd_pos.push_back(0);
    }
    _fd_alive[new_fd] = 1;
    _fd_pos[new_fd] = _max_pos;
    _live_fds++;

    fd = new_fd;
    // no need to SELECT_WRITE; flush(fd) will do it if required

    if (_greeting) {
      // XXX - assume that this write will succeed
      String s = String("Click::ChatterSocket/") + protocol_version + "\r\n";
      int w = write(fd, s.data(), s.length());
      if (w != s.length())
	click_chatter("%s fd %d: unable to write greeting!", declaration().c_str(), fd);
    }
  }

  flush(fd);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ChatterSocket)
