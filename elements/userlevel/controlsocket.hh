#ifndef CONTROLSOCKET_HH
#define CONTROLSOCKET_HH
#include <click/element.hh>

/*
 * =c
 * ControlSocket(tcp, PORTNUMBER [, READONLY?])
 * ControlSocket(unix, FILENAME [, READONLY?])
 * =s
 * opens control sockets for other programs
 * V<debugging>
 * =io
 * None
 * =d
 *
 * Opens a control socket that allows other user-level programs to call read
 * or write handlers on the router. The two forms of the command open a TCP
 * connection that listens to port PORTNUMBER, or a UNIX-domain socket that
 * listens on file FILENAME. Disallows write handlers if READONLY? is true; it
 * is false by default.
 *
 * The "server" (that is, the ControlSocket element) speaks a relatively
 * simple line-based protocol. Commands sent to the server are single lines of
 * text; they consist of words separated by spaces. The server responds to
 * every command with at least one message line followed optionally by some
 * data. Message lines start with a three-digit response code, as in FTP. When
 * multiple message lines are sent in response to a single command, all but
 * the last begin with the response code and a hyphen (as in "200-Hello!");
 * the last line begins with the response code and a space (as in "200
 * Hello!").
 *
 * The server will accept lines terminated by CR, LF, or CRLF. Its response
 * lines are always terminated by CRLF.
 *
 * When a connection is opened, the server responds by stating its protocol
 * version number with a line like "Click::ControlSocket/1.0". The current
 * version number is 1.0.
 *
 * The server currently supports the following commands:
 *
 * =over 5
 *
 * =item READ I<element>.I<handlername>
 *
 * Call the read handler named I<handlername> on the element named I<element>
 * and return the results. On success, responds with a "success" message
 * (response code 2xy) followed by a line like "DATA I<n>". Here, I<n> is a
 * decimal integer indicating the length of the read handler data. The I<n>
 * bytes immediately following (the CRLF that terminates) the DATA line are
 * the handler's results.
 *
 * =item READ I<handlername>
 *
 * Call the global read handler named I<handlername> and return the results as
 * in READ I<element>.I<handlername>. There are seven global read handlers,
 * named `version', `list', `classes', `config', `flatconfig', `packages', and
 * `requirements'. See the handlers of the same name in click.o(8) for more
 * information.
 *
 * =item WRITE I<element>.I<handlername> I<args...>
 *
 * Call the write handler named I<handlername> on the element named
 * I<element>, passing the I<args> (if any) as arguments.
 *
 * =item WRITEDATA I<element>.I<handlername> I<n>
 *
 * Call the write handler named I<handlername> on the element named
 * I<element>. The arguments to pass are the I<n> bytes immediately following
 * (the CRLF that terminates) the WRITEDATA line.
 *
 * =item QUIT
 *
 * Close the connection.
 *
 * =back
 *
 * The server's response codes follow this pattern.
 *
 * =over 5
 *
 * =item 2xy
 * The command succeeded.
 *
 * =item 5xy
 * The command failed.
 *
 * =back
 *
 * Here are some of the particular error messages:
 *
 * 200 OK.
 *
 * 220 OK, but the handler reported some warnings.
 *
 * 500 Syntax error.
 *
 * 501 Unimplemented command.
 *
 * 510 No such element.
 *
 * 511 No such handler.
 *
 * 520 Handler error.
 *
 * 530 Permission denied.
 *
 * Only available in user-level processes.
 *
 * =e
 *   ControlSocket(unix, /tmp/clicksocket); */

class ControlSocket : public Element {

  String _unix_pathname;
  int _socket_fd;
  bool _read_only;
  Vector<String> _in_texts;
  Vector<String> _out_texts;
  Vector<int> _flags;

  enum { READ_CLOSED = 1, WRITE_CLOSED = 2 };
  static const int ANY_ERR = -1;
  static const int NAME_ERR = -2;

  static const char *protocol_version;

  int global_read_handler(int, const String &, String *);
  int parse_handler(int fd, const String &, Element **);
  int write_command(int fd, const String &, const String &);
  int parse_command(int fd, const String &);

 public:

  ControlSocket()			{ }

  const char *class_name() const	{ return "ControlSocket"; }
  ControlSocket *clone() const		{ return new ControlSocket; }
  
  int configure(const Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void selected(int);

  int message(int fd, int code, const String &, bool continuation = false);
  
};

extern String click_userlevel_classes_string();
extern String click_userlevel_config_string();
extern String click_userlevel_packages_string();

#endif
