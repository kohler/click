#ifndef CONTROLSOCKET_HH
#define CONTROLSOCKET_HH
#include "elements/userlevel/handlerproxy.hh"
class ControlSocketErrorHandler;

/*
=c

ControlSocket("TCP", PORTNUMBER [, READONLY?, I<KEYWORDS>])
ControlSocket("UNIX", FILENAME [, READONLY?, I<KEYWORDS>])

=s debugging

opens control sockets for other programs

=io

None

=d

Opens a control socket that allows other user-level programs to call read or
write handlers on the router. Depending on its configuration string,
ControlSocket will listen on TCP port PORTNUMBER, or on a UNIX-domain socket
named FILENAME. Disallows write handlers if READONLY? is true (it is false by
default). With the PROXY keyword argument, you can make ControlSocket speak to
a kernel driver; see below.

The "server" (that is, the ControlSocket element) speaks a relatively
simple line-based protocol. Commands sent to the server are single lines of
text; they consist of words separated by spaces. The server responds to
every command with at least one message line followed optionally by some
data. Message lines start with a three-digit response code, as in FTP. When
multiple message lines are sent in response to a single command, all but
the last begin with the response code and a hyphen (as in "200-Hello!");
the last line begins with the response code and a space (as in "200
Hello!").

The server will accept lines terminated by CR, LF, or CRLF. Its response
lines are always terminated by CRLF.

When a connection is opened, the server responds by stating its protocol
version number with a line like "Click::ControlSocket/1.1". The current
version number is 1.1. Changes in minor version number will only add commands
and functionality to this specification, not change existing functionality.

Keyword arguments are:

=over 8

=item READONLY

Boolean. Same as the READONLY? argument.

=item PROXY

String. Specifies an element proxy. When a user requests the value of handler
E.H, ControlSocket will actually return the value of `PROXY.E.H'. This is
useful with elements like KernelHandlerProxy. Default is empty (no proxy).

=item VERBOSE

Boolean. When true, ControlSocket will print messages whenever it accepts a
new connection or drops an old one. Default is false.

=back

=head1 SERVER COMMANDS

The server currently supports the following commands:

=over 5

=item READ I<element>.I<handlername>

Call the read handler named I<handlername> on the element named I<element>
and return the results. On success, responds with a "success" message
(response code 2xy) followed by a line like "DATA I<n>". Here, I<n> is a
decimal integer indicating the length of the read handler data. The I<n>
bytes immediately following (the CRLF that terminates) the DATA line are
the handler's results.

I<element> is either an element name or an integer, where the integer is the
index of the element, starting from 1.

=item READ I<handlername>

Call the global read handler named I<handlername> and return the results as
in READ I<element>.I<handlername>. There are seven global read handlers,
named `version', `list', `classes', `config', `flatconfig', `packages', and
`requirements'. See the handlers of the same name in click.o(8) for more
information.

A global handler named I<handlername> may also be referred to as `C<0.>I<handlername>'.

=item WRITE I<element>.I<handlername> I<args...>

Call the write handler named I<handlername> on the element named
I<element>, passing the I<args> (if any) as arguments.

=item WRITEDATA I<element>.I<handlername> I<n>

Call the write handler named I<handlername> on the element named
I<element>. The arguments to pass are the I<n> bytes immediately following
(the CRLF that terminates) the WRITEDATA line.

You can also write a global handler with WRITE or WRITEDATA.

=item CHECKREAD I<handler>

Returns 1 if I<handler> exists and is readable; 0 if it does not exist, or is write-only; and -1 if ControlSocket can't tell without invoking the handler. I<handler> may be an element handler or a global handler.

=item CHECKWRITE I<handler>

Returns 1 if I<handler> exists and is writable; 0 if it does not exist, or is read-only; and -1 if ControlSocket can't tell without invoking the handler. I<handler> may be an element handler or a global handler.

=item QUIT

Close the connection.

=back

The server's response codes follow this pattern.

=over 5

=item 2xy
The command succeeded.

=item 5xy
The command failed.

=back

Here are some of the particular error messages:

  200 OK.
  220 OK, but the handler reported some warnings.
  500 Syntax error.
  501 Unimplemented command.
  510 No such element.
  511 No such handler.
  520 Handler error.
  530 Permission denied.
  540 No router installed.

ControlSocket is only available in user-level processes.

=e

  ControlSocket(unix, /tmp/clicksocket);

=a ChatterSocket, KernelHandlerProxy */

class ControlSocket : public Element { public:

  ControlSocket();
  ~ControlSocket();

  const char *class_name() const	{ return "ControlSocket"; }
  ControlSocket *clone() const		{ return new ControlSocket; }
  
  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void selected(int);

  int message(int fd, int code, const String &, bool continuation = false);
  int transfer_messages(int fd, int default_code, const String &first_message,
			ControlSocketErrorHandler *);
  
  enum {
    CSERR_OK			= HandlerProxy::CSERR_OK,	       // 200
    CSERR_OK_HANDLER_WARNING	= 220,
    CSERR_SYNTAX		= HandlerProxy::CSERR_SYNTAX,          // 500
    CSERR_UNIMPLEMENTED		= 501,
    CSERR_NO_SUCH_ELEMENT	= HandlerProxy::CSERR_NO_SUCH_ELEMENT, // 510
    CSERR_NO_SUCH_HANDLER	= HandlerProxy::CSERR_NO_SUCH_HANDLER, // 511
    CSERR_HANDLER_ERROR		= HandlerProxy::CSERR_HANDLER_ERROR,   // 520
    CSERR_DATA_TOO_BIG		= 521,
    CSERR_PERMISSION		= HandlerProxy::CSERR_PERMISSION,      // 530
    CSERR_NO_ROUTER		= HandlerProxy::CSERR_NO_ROUTER,       // 540
    CSERR_UNSPECIFIED		= HandlerProxy::CSERR_UNSPECIFIED,     // 590
  };
  
 private:

  String _unix_pathname;
  int _socket_fd;
  bool _read_only : 1;
  bool _verbose : 1;
  Element *_proxy;
  HandlerProxy *_full_proxy;
  
  Vector<String> _in_texts;
  Vector<String> _out_texts;
  Vector<int> _flags;

  String _proxied_handler;
  ErrorHandler *_proxied_errh;

  enum { READ_CLOSED = 1, WRITE_CLOSED = 2 };
  static const int ANY_ERR = -1;

  static const char * const protocol_version;

  String proxied_handler_name(const String &) const;
  int parse_handler(int fd, const String &, Element **);
  int read_command(int fd, const String &);
  int write_command(int fd, const String &, const String &);
  int check_command(int fd, const String &, bool write);
  int parse_command(int fd, const String &);

  int report_proxy_errors(int fd, const String &);
  static ErrorHandler *proxy_error_function(const String &, void *);

};

#endif
