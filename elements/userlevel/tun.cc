/*
 * tun.{cc,hh} -- element accesses network via /dev/tun device
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
#include "tun.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "glue.hh"
#include "elements/standard/scheduleinfo.hh"
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

Tun::Tun()
{
  add_input();
  add_output();
  _fd = -1;
}

Tun::~Tun()
{
}

Tun *
Tun::clone() const
{
  return new Tun();
}

int
Tun::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "device name prefix", &_dev_prefix,
		  cpIPAddress, "near address", &_near,
		  cpIPAddress, "far address", &_far,
		  cpEnd) < 0)
    return -1;

  return 0;
}

int
Tun::initialize(ErrorHandler *errh)
{
  _fd = alloc_tun(_dev_prefix.cc(), _near, _far, errh);
  if (_fd < 0)
    return -1;
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
Tun::uninitialize()
{
  unschedule();
  if (_fd >= 0)
    close(_fd);
}

void
Tun::selected(int fd)
{
  int cc;
  char b[2048];

  if (fd != _fd)
    return;
  
  cc = read(_fd, b, sizeof(b));
  if(cc > 0){
#if defined (__OpenBSD__)
    Packet *p = Packet::make(b+4, cc-4);
#elif defined (__linux__)
    Packet *p = Packet::make(b+16, cc-16);
#else
    Packet *p = Packet::make(b, cc);
#endif
    output(0).push(p);
  } else {
    perror("Tun read");
  }
}

void
Tun::run_scheduled()
{
  if (Packet *p = input(0).pull()) {
    push(0, p); 
  }
  reschedule();
}

void
Tun::push(int, Packet *p)
{
#if defined (__OpenBSD__)
  char big[2048];
  int af;

  if(p->length()+4 >= sizeof(big)){
    fprintf(stderr, "bimtun writetun pkt too big\n");
    return;
  }
  af = htonl(AF_INET);
  memcpy(big, &af, 4);
  memcpy(big+4, p->data(), p->length());
  if(write(_fd, big, p->length()+4) != (int)p->length()+4){
    perror("write tun");
  }
#elif defined(__linux__)
  /*
   * Ethertap is linux equivalent of Tun; wants ethernet header plus 2
   * alignment bytes 
   */
  char big[2048];

  if(p->length()+16 >= sizeof(big)){
    fprintf(stderr, "bimtun writetun pkt too big\n");
    return;
  }
  bzero(big, 16);
  memcpy(big+16, p->data(), p->length());
  if(write(_fd, big, p->length()+16) != (int)p->length()+16){
    perror("write tun");
  }
#else
  if(write(_fd, p->data(), p->length()) != (int) p->length()){
    perror("write tun");
  }
#endif

  p->kill();
}

/*
 * Find an kill()d /dev/tun* device, return a fd to it.
 * Exits on failure.
 */
int
Tun::alloc_tun(const char *dev_prefix, struct in_addr near, struct in_addr far,
               ErrorHandler *errh)
{
  int i, fd, yes = 1;
  char tmp[512], tmp0[64], tmp1[64];;

  for(i = 0; i < 32; i++){
    sprintf(tmp, "/dev/%s%d", dev_prefix, i);
    fd = open(tmp, 2);
    if(fd >= 0){
#ifdef TUNSIFINFO
      struct tuninfo ti;
      if(ioctl(fd, TUNGIFINFO, &ti) < 0){
        close(fd);
        return errh->error("TUNGIFINFO failed");
      }
      ti.mtu = 576;
      if(ioctl(fd, TUNSIFINFO, &ti) < 0){
        close(fd);
        return errh->error("TUNSIFINFO failed");
      }
#endif
#ifdef FIONBIO
      if(ioctl(fd, FIONBIO, &yes) < 0){
        close(fd);
        return errh->error("FIONBIO failed");
      }
#else
      return errh->error("not configured for non-blocking IO");
#endif

      strcpy(tmp0, inet_ntoa(near));
      strcpy(tmp1, inet_ntoa(far));
#if defined(__linux__)
      // XXX don't know what to do with far address 
      sprintf(tmp, "ifconfig %s%d %s up", dev_prefix, i, tmp0);
#else
      sprintf(tmp, "ifconfig %s%d %s %s up", dev_prefix, i, tmp0, tmp1);
#endif
      if(system(tmp) != 0){
        close(fd);
        return errh->error("failed: %s", tmp);
      }
      return(fd);
    }
  }

  return errh->error("could not allocate a free /dev/%s* device", dev_prefix);
}

EXPORT_ELEMENT(Tun)
