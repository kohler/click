#ifndef CONTROLSOCKET_HH
#define CONTROLSOCKET_HH
#include "element.hh"

/*
 * read ELEMENT.HANDLERNAME
 * write ELEMENT.HANDLERNAME DATA
 * writex ELEMENT.HANDLERNAME NBYTES
 */

class ControlSocket : public Element {

  String _unix_pathname;
  int _socket_fd;
  Vector<String> _in_texts;
  Vector<String> _out_texts;
  Vector<int> _flags;

  enum { READ_CLOSED = 1, WRITE_CLOSED = 2 };

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

#endif
