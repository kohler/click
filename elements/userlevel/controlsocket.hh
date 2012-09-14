#ifndef CLICK_CONTROLSOCKET_HH
#define CLICK_CONTROLSOCKET_HH
#include "elements/userlevel/handlerproxy.hh"
#include <click/straccum.hh>
CLICK_DECLS
class ControlSocketErrorHandler;
class Timer;
class Handler;

/*
=c

ControlSocket("TCP", PORTNUMBER [, I<keywords READONLY, PROXY, VERBOSE, LOCALHOST, RETRIES, RETRY_WARNINGS>])
ControlSocket("UNIX", FILENAME [, I<keywords>])

=s control

opens control sockets for other programs

=d

Opens a control socket that allows other user-level programs to call read or
write handlers on the router. Depending on its configuration string,
ControlSocket will listen on TCP port PORTNUMBER, or on a UNIX-domain socket
named FILENAME. With the PROXY keyword argument, you can make ControlSocket speak to
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
version number with a line like "Click::ControlSocket/1.3". The current
version number is 1.3. Changes in minor version number will only add commands
and functionality to this specification, not change existing functionality.

ControlSocket supports hot-swapping, meaning you can change configurations
without interrupting existing clients. The hot-swap will succeed only if the
old ControlSocket and the new ControlSocket have the same element name, and
the same socket type and port/filename parameters. Additionally, the new
ControlSocket must have RETRIES set to 1 or more, since the old ControlSocket
has already bound the relevant socket.

Keyword arguments are:

=over 8

=item READONLY

Boolean.  Disallows write handlers if true (it is false by
default).

=item PROXY

String. Specifies an element proxy. When a user requests the value of handler
E.H, ControlSocket will actually return the value of `PROXY.E.H'. This is
useful with elements like KernelHandlerProxy. Default is empty (no proxy).

=item VERBOSE

Boolean. When true, ControlSocket will print messages whenever it accepts a
new connection or drops an old one. Default is false.

=item LOCALHOST

Boolean. When true, a TCP ControlSocket will only accept connections from the
local host (127.0.0.1). Default is false.

=item RETRIES

Integer. If greater than 0, ControlSocket won't immediately fail when it can't
open its socket. Instead, it will attempt to open the socket once a second
until it succeeds, or until RETRIES unsuccessful attempts (after which it will
stop the router). Default is 0.

=item RETRY_WARNINGS

Boolean. If true, ControlSocket will print warning messages every time it
fails to open a socket. If false, it will print messages only on the final
failure. Default is true.

=back

The PORT argument for TCP ControlSockets can also be an integer followed by a
plus sign, as in "ControlSocket(TCP, 41930+)".  This means that the
ControlSocket should bind on some port I<greater than or equal to> PORT.  If
PORT itself is in use, ControlSocket will try several nearby ports before
giving up.  This can be useful in tests.

=head1 SERVER COMMANDS

Many server commands
take a I<handler> argument. These arguments name handlers, and take one of
three forms: C<I<elementname>.I<handlername>> names a particular element's
handler; C<I<elementnumber>.I<handlername>> also names an element handler, but
the element is identified by index, starting from 1; and C<I<handlername>>
names a global handler. (There are seven global read handlers, named
C<version>, C<list>, C<classes>, C<config>, C<flatconfig>, C<packages>, and
C<requirements>. See click.o(8) for more information.)

=over 5

=item READ I<handler> [I<params...>]

Call a read I<handler>, passing the I<params>, if any,
as arguments, and return the results.
On success, responds with a "success" message (response
code 2xy) followed by a line like "DATA I<n>". Here, I<n> is a
decimal integer indicating the length of the read handler data. The I<n>
bytes immediately following (the CRLF that terminates) the DATA line are
the handler's results.

=item READDATA I<handler> I<n>

Call a read I<handler>, passing the I<n> bytes immediately following (the CRLF
that terminates) the READDATA line as arguments, and return the results with
"DATA I<n>" as in the READ command. Introduced in version 1.2 of the
ControlSocket protocol.

=item READUNTIL I<handler> I<terminator>

Call a read I<handler> and return the results with "DATA I<n>" as in the READ
command. Parameters for I<handler> are read from the input stream. Parameter
reading stops at the first line that equals I<terminator>. If I<terminator> is
not supplied, parameter reading stops at the first blank line. When searching
for a terminator, ControlSocket removes trailing spaces from both
I<terminator> and the input lines. Introduced in version 1.3 of the
ControlSocket protocol.

=item WRITE I<handler> I<params...>

Call a write I<handler>, passing the I<params>, if any, as arguments.

=item WRITEDATA I<handler> I<n>

Call a write I<handler>. The arguments to pass are the I<n> bytes immediately
following (the CRLF that terminates) the WRITEDATA line.

=item WRITEUNTIL I<handler> I<terminator>

Call a write I<handler>. The arguments to pass are the read from the input
stream, stopping at the first line that equals I<terminator>.

=item CHECKREAD I<handler>

Checks whether a I<handler> exists and is readable. The return status is 200
for readable handlers, and an appropriate error status for non-readable
handlers or nonexistent handlers.

=item CHECKWRITE I<handler>

Checks whether a I<handler> exists and is writable.

=item LLRPC I<llrpc> [I<n>]

Call an LLRPC I<llrpc> and return the results. I<Llrpc> should have the form
C<I<element>#I<hexnumber>>. The C<I<hexnumber>> is the LLRPC number, from
C<E<lt>click/llrpc.hE<gt>>, in hexadecimal network format. Translate C<CLICK_LLRPC>
constants to network format by calling
C<CLICK_LLRPC_HTON(CLICK_LLRPC_...)>. If I<n> is given, then the I<n> bytes
immediately following (the CRLF that terminates) the LLRPC line are passed in
as an argument. The results are returned after a "DATA I<nn>" line, as in
READ.

ControlSocket will not call an LLRPC unless it can determine (from the command
number) how much data the LLRPC expects and returns. (Only "flat" LLRPCs may
be called; they are declared using the _CLICK_IOC_[RWS]F macros.)

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

=h port r

Returns the ControlSocket's port number.  Only available for TYPE TCP.

=h filename r

Returns the ControlSocket's UNIX socket filename.  Only available for TYPE
UNIX.

=a ChatterSocket, KernelHandlerProxy */

class ControlSocket : public Element { public:

    ControlSocket() CLICK_COLD;
    ~ControlSocket() CLICK_COLD;

    const char *class_name() const	{ return "ControlSocket"; }

    int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    Element *hotswap_element() const;
    void take_state(Element *, ErrorHandler *);
    void add_handlers() CLICK_COLD;

    void selected(int fd, int mask);

    enum {
	CSERR_OK			= HandlerProxy::CSERR_OK,	       // 200
	CSERR_OK_HANDLER_WARNING	= 220,
	CSERR_SYNTAX			= HandlerProxy::CSERR_SYNTAX,          // 500
	CSERR_UNIMPLEMENTED		= 501,
	CSERR_NO_SUCH_ELEMENT		= HandlerProxy::CSERR_NO_SUCH_ELEMENT, // 510
	CSERR_NO_SUCH_HANDLER		= HandlerProxy::CSERR_NO_SUCH_HANDLER, // 511
	CSERR_HANDLER_ERROR		= HandlerProxy::CSERR_HANDLER_ERROR,   // 520
	CSERR_DATA_TOO_BIG		= 521,
	CSERR_LLRPC_ERROR		= 522,
	CSERR_PERMISSION		= HandlerProxy::CSERR_PERMISSION,      // 530
	CSERR_NO_ROUTER			= HandlerProxy::CSERR_NO_ROUTER,       // 540
	CSERR_UNSPECIFIED		= HandlerProxy::CSERR_UNSPECIFIED      // 590
    };

  private:

    enum { type_tcp, type_unix, type_socket };

    String _unix_pathname;
    int _socket_fd;
    bool _read_only : 1;
    bool _verbose : 1;
    bool _retry_warnings : 1;
    bool _localhost : 1;
    uint8_t _type;
    Element *_proxy;
    HandlerProxy *_full_proxy;

    struct connection {
	int fd;
	StringAccum in_text;
	int inpos;
	StringAccum out_text;
	int outpos;
	bool in_closed;
	bool out_closed;
	connection(int fd_)
	    : fd(fd_), inpos(0), outpos(0),
	      in_closed(false), out_closed(false) {
	}
	int message(int code, const String &msg, bool continuation = false);
	int transfer_messages(int default_code, const String &msg, ControlSocketErrorHandler *);
	static void contract(StringAccum &sa, int &pos);
	void flush_write(ControlSocket *cs, bool read_needs_processing);
	int read(int len, String &data);
	int read_insufficient();
    };
    Vector<connection *> _conns;

    String _proxied_handler;
    ErrorHandler *_proxied_errh;

    int _retries;
    Timer *_retry_timer;

    enum { READ_CLOSED = 1, WRITE_CLOSED = 2, ANY_ERR = -1 };

    static const char protocol_version[];

    int initialize_socket_error(ErrorHandler *, const char *);
    int initialize_socket(ErrorHandler *);
    static void retry_hook(Timer *, void *);
    void initialize_connection(int fd);

    String proxied_handler_name(const String &) const;
    const Handler* parse_handler(connection &conn, const String &, Element **);
    int read_command(connection &conn, const String &, String);
    int write_command(connection &conn, const String &, String);
    int check_command(connection &conn, const String &, bool write);
    int llrpc_command(connection &conn, const String &, String);
    int parse_command(connection &conn, const String &);

    static ErrorHandler *proxy_error_function(const String &, void *);

};

CLICK_ENDDECLS
#endif
