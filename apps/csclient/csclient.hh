/*
 * csclient.{cc,hh} -- class for connecting to Click ControlSockets.
 * Douglas S. J. De Couto <decouto@lcs.mit.edu>
 * based on the author's ControlSocket.java which was improved by Eddie Kohler
 */

#include <string>
#include <unistd.h>

class ControlSocketClient
{
private:
  int _init;
  
  unsigned int _host;
  unsigned short _port;
  int _fd;
  int _protocol_minor_version;

  string _name;

  static const int CODE_OK = 200;
  static const int CODE_OK_WARN = 220;
  static const int CODE_SYNTAX_ERR = 500;
  static const int CODE_UNIMPLEMENTED = 501;
  static const int CODE_NO_ELEMENT = 510;
  static const int CODE_NO_HANDLER = 511;
  static const int CODE_HANDLER_ERR = 520;
  static const int CODE_PERMISSION = 530;
  static const int CODE_NO_ROUTER = 540;
  
  static const int PROTOCOL_MAJOR_VERSION = 1;
  static const int PROTOCOL_MINOR_VERSION = 0;
  
  static const int _sock_timeout = 0; // msecs

  /* Try to read a '\n'-terminated line (including the '\n') from the
   * socket.  Returns -1 if there was an error. */
  int readline(string &buf);

  int get_resp_code(string line);
  int get_data_len(string line);

public:
  ControlSocketClient() : _init(false), _fd(0) { }
  
  /* HOST is IP address in network byte order.  Returns 0 iff okay,
   * else -1 */
  int configure(unsigned int host, unsigned short port);

  const string name() { return _name; }
  
  /* Read handler.  Returns the the results of calling handler
   * ``NAME.HANDLER''; if NAME is empty, return the result of just
   * calling ``HANDLER'' (i.e. don't put the `.' in). */
  int read(string name, string handler, string &response);

  ~ControlSocketClient() { if (_init) ::close(_fd); }

};
