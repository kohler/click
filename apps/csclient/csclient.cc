/* 
 * csclient.cc
 * Douglas S. J. De Couto
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>

#include "csclient.hh"

int
ControlSocketClient::configure(unsigned int ip, unsigned short port)
{
  assert(!_init);
  
  _host = ip;
  _port = port;
  
  _fd = socket(PF_INET, SOCK_STREAM, 0);
  if (_fd < 0) 
    return -1;
  
  /* 
   * connect to remote ControlSocket
   */
  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = _host;
  sa.sin_port = htons(port);
  
  char namebuf[32];
  snprintf(namebuf, 32, "%u.%u.%u.%u:%hu", 
	   (ip & 0xff) >> 0,
	   (ip & 0xff00) >> 8,
	   (ip & 0xff0000) >> 16,
	   (ip & 0xff000000) >> 24,
	   port);
  _name = namebuf;
  
  int res = connect(_fd, (struct sockaddr *)  &sa, sizeof(sa));
  if (res < 0) 
    return -1;
  
  int major, minor;
  unsigned int slash, dot;

  /* 
   * check that we get the expected banner
   */
  string buf;
  res = readline(buf);
  if (res < 0)
    goto err;
  
  slash = buf.find_first_of('/');
  dot = (slash != string::npos ? buf.find_first_of('.', slash + 1) : string::npos);
  if (slash == string::npos || dot == string::npos)
    goto err; /* bad format */
  
  /*
   * check ControlSocket protocol version
   */
  major = atoi(buf.substr(slash + 1, dot - slash - 1).c_str());
  minor = atoi(buf.substr(dot + 1, buf.size() - dot - 1).c_str());
  if (major != PROTOCOL_MAJOR_VERSION ||
      minor < PROTOCOL_MINOR_VERSION)
    goto err; /* wrong version */
  
  _init = true;
  return 0;

 err:
  close(_fd);
  return -1;
}


int
ControlSocketClient::readline(string &buf)
{
  assert(_fd);

  /* 
   * keep calling read() to get one character at a time, until we get
   * a line.  not very ``efficient'', but who cares?  
   */
  char c = 0;
  buf.resize(0);
  do {
    int res = ::read(_fd, (void *) &c, 1);
    if (res < 0)
      return -1;
    assert(res == 1);
    buf += c;
  } 
  while (c != '\n');

  return 0;
}


int
ControlSocketClient::get_resp_code(string line)
{
  if (line.size() < 3)
    return -1;
  return atoi(line.substr(0, 3).c_str());
}


int
ControlSocketClient::get_data_len(string line)
{
  unsigned int i;
  for (i = 0; i < line.size() && !isdigit(line[i]); i++)
    ; // scan string
  if (i == line.size())
    return -1;
  return atoi(line.substr(i, line.size() - i).c_str());
}


int
ControlSocketClient::read(string name, string handler, string &response)
{
  assert(_init);
  
  if (name.size() > 0)
    handler = name + "." + handler;
  string cmd = "READ " + handler + "\n";
  
  /*
   * write command string)
   */
  int res = write(_fd, cmd.c_str(), cmd.size());
  if (res < 0)
    return -1;
  assert(res == (int) cmd.size());

  string cmd_resp;
  string line;
  do {
    res = readline(line);
    if (res < 0)
      return -1;
    if (line.size() < 4)
      return -1;
    cmd_resp += line;
  }
  while (line[3] == '-');
    
  int code = get_resp_code(line);
  if (code != CODE_OK && code != CODE_OK_WARN) 
    return -code;
  
  res = readline(line);
  if (res < 0)
    return -1;
  int num = get_data_len(line);
  if (num < 0)
    return -1;

  response.resize(0);
  if (num == 0)
    return 0;
  
  char *buf = new char[num];
  int num_read = 0;
  while (num_read < num) {
    res = ::read(_fd, buf + num_read, num - num_read);
    if (res < 0) {
      delete[] buf;
      return -1;
    }
    num_read += res;
  }

  response.append(buf, num);
  delete[] buf;

  return 0;
}
