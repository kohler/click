#ifndef USERWRITEHANDLERS_HH
#define USERWRITEHANDLERS_HH
#include "element.hh"
#include "timer.hh"

/*
 * =c
 * UserWriteHandlers(SOCK [, MAXCMDLEN])
 * =io
 * None
 * =d
 *
 * the args are really: (SOCK, PERIOD [, MAXCMDLEN) because i can't
 * get the select-based implementation working right now.
 *
 * Runs write handlers as read from the UNIX domain socket named SOCK.
 * Command strings are of the form ``handler-name;handler-arg\n'', causing
 * handler-name(handler-arg) to be run.
 *
 * The number of chars in the command string must be less than
 * MAXCMDLEN, which defaults to 255.  
 *
 * See ~decouto/src/writeunixsock.cc for a program that will write
 * properly formatted commands to this element.
 */

/* XXX hmmm... the select implementation was working briefly but no it
   stopped, once i put in the UWH_USE_SELECT macro... */
#define UWH_USE_SELECT 0

class UserWriteHandlers : public Element {

#if !UWH_USE_SELECT
  Timer _timer;
  int _period; // msecs
#endif
  String _fname;
  int _buflen;
  char *_buf;
  int _fd;
  int _pos;

  static void timer_hook(unsigned long);
  int read_command_strings(String &hndlr_str, String &arg_str, ErrorHandler *errh);
  
 public:

  UserWriteHandlers();
  ~UserWriteHandlers();

  const char *class_name() const		{ return "UserWriteHandlers"; }
  UserWriteHandlers *clone() const		{ return new UserWriteHandlers; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

#if UWH_USE_SELECT
  int select_fd() const { return _fd; }
  void selected(int fd);
#endif

};

#endif
