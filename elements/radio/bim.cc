/*
 * bim.{cc,hh} -- element reads and writes packets to/from
 * ABACOM BIM-4xx-RS232 radios
 * Robert Morris
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
#include "bim.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "glue.hh"
#include "bim-proto.hh"
#include "elements/standard/scheduleinfo.hh"
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

BIM::BIM()
{
  add_input();
  add_output();
  _speed = 9600;
  _fd = -1;
  _len = _started = _escaped = 0;
}

BIM::~BIM()
{
  if(_fd >= 0)
    close(_fd);
}

BIM *
BIM::clone() const
{
  return new BIM();
}

int
BIM::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "device name", &_dev,
		  cpOptional,
		  cpInteger, "baud rate", &_speed,
		  cpEnd) < 0)
    return -1;

  if(_speed == 4800)
    _speed = B4800;
  else if(_speed == 9600)
    _speed = B9600;
  else if(_speed == 19200)
    _speed = B19200;
  else if(_speed == 38400)
    _speed = B38400;
  else {
    return errh->error("bad speed %d", _speed);
  }

  return 0;
}

int
BIM::initialize(ErrorHandler *errh)
{
  if (_fd >= 0)
    return 0;
  else if (!_dev)
    return errh->error("device name not set");
  
  char *dname = _dev.mutable_c_str();
  _fd = open(dname, O_RDWR|O_NONBLOCK, 0);
  if(_fd < 0)
    return errh->error("%s: %s", dname, strerror(errno));

  ioctl(_fd, TIOCEXCL, 0);

  struct termios t;

  if(tcgetattr(_fd, &t) < 0)
    return errh->error("bad tcgetattr");

  t.c_iflag = IGNBRK;
  t.c_oflag = 0;
  t.c_cflag = CS8|CREAD|HUPCL|CLOCAL;
  t.c_lflag = 0;
  cfsetispeed(&t, _speed);
  cfsetospeed(&t, _speed);
  if(tcsetattr(_fd, TCSANOW, &t) < 0)
    return errh->error("can't set terminal characteristics");

#ifdef FIONBIO
  int yes = 1;
  if(ioctl(_fd, FIONBIO, &yes) < 0)
    return errh->error("can't set non-blocking IO");
#else
  return errh->error("not configured for non-blocking IO");
#endif

#ifdef TCIOFLUSH
  tcflush(_fd, TCIOFLUSH);
#elif defined(TIOCFLUSH)
  tcflush(_fd, TIOCFLUSH);
#else
  return errh->error("this architecture has no TIOCFLUSH");
#endif

  ScheduleInfo::join_scheduler(this, errh);
  add_select(_fd, SELECT_READ | SELECT_WRITE);
  
  return 0;
}

void
BIM::uninitialize()
{
  unschedule();
  remove_select(_fd, SELECT_READ | SELECT_WRITE);
}

void
BIM::selected(int fd)
{
  int cc, i;
  char b[128];
  
  if (fd != _fd)
    return;

  cc = read(_fd, b, sizeof(b));
  for(i = 0; i < cc; i++)
    got_char(b[i] & 0xff);
}

void
BIM::got_char(int c)
{
  if(_started == 0 && _escaped == 0 && c == BIM_ESC){
    _escaped = 1;
  } else if(_escaped && c == BIM_ESC_START){
    _escaped = 0;
    _started = 1;
    _len = 0;
  } else if(_started && !_escaped && c == BIM_ESC){
    _escaped = 1;
  } else if(_started && _escaped && c == BIM_ESC_END){
    Packet *p = Packet::make(_buf, _len);
    output(0).push(p);
    _escaped = _started = _len = 0;
  } else if(_started && _escaped && c == BIM_ESC_ESC){
    _buf[_len++] = BIM_ESC;
    _escaped = 0;
  } else if(_started && !_escaped && c != BIM_ESC){
    _buf[_len++] = c;
  } else if(_started || _escaped){
    fprintf(stderr, "bim: framing error, st %d es %d _len %d c %02x\n",
            _started, _escaped, _len, c);
    _started = _escaped = _len = 0;
  }

  if(_len > 1024){
    fprintf(stderr, "bim: incoming packet too large\n");
    _len = _started = 0;
  }
}

void
BIM::run_scheduled()
{
  if (Packet *p = input(0).pull())
  {
    push(0, p); 
    reschedule();
  } 
}

void
BIM::push(int, Packet *p)
{
  send_packet(p->data(), p->length());
  p->kill();
}

void
BIM::send_packet(const unsigned char buf[], unsigned int len)
{
  unsigned int i, j;
  char big[2048];

  if(len > 1024){
    fprintf(stderr, "bim: packet too large\n");
    return;
  }

  j = 0;
  for(i = 0; i < 13; i++){
    big[j++] = 'U'; /* preamble 01010101 */
  }
  big[j++] = 0xff; /* sync */

  big[j++] = BIM_ESC;
  big[j++] = BIM_ESC_START;

  for(i = 0; i < len; i++){
    int c = buf[i] & 0xff;
    if(c == BIM_ESC){
      big[j++] = BIM_ESC;
      big[j++] = BIM_ESC_ESC;
    } else {
      big[j++] = c;
    }
  }

  big[j++] = BIM_ESC;
  big[j++] = BIM_ESC_END;

  if(write(_fd, big, j) != (int) j){
    perror("bim: write rs232");
  }

#if 0
  {
    FILE *fp = fopen("foo", "a");
    if(fp){
      double a = 0.5;
      int by, bi;
      for(by = 0; by < j; by++){
        for(bi = 0; bi < 8; bi++){
          int bit = (big[by] >> bi) & 1;
          a = (a * 0.9) + (bit * 0.1);
          fprintf(fp, "%.5f\n", a);
        }
      }
      fprintf(fp, "\n");
      fclose(fp);
    }
  }
#endif
}

EXPORT_ELEMENT(BIM)
ELEMENT_REQUIRES(userlevel)
