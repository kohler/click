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
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <net/if.h>
#include <net/if_tun.h>
#endif

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
		  cpString, "device name prefix", &_dev_name,
		  cpIPAddress, "address", &_near,
		  cpIPAddress, "netmask", &_mask,
		  cpOptional,
		  cpIPAddress, "default gateway", &_gw,
		  cpEnd) < 0)
    return -1;

  if (_gw) { // then it was set to non-zero by arg
    // check net part matches 
    unsigned int g = _gw.in_addr().s_addr;
    unsigned int m = _mask.in_addr().s_addr;
    unsigned int n = _near.in_addr().s_addr;
    if ((g & m) != (n & m)) {
      _gw = 0;
      errh->warning("%s: not setting up default route, tun address and gateway address are on different networks", id().cc());
    }
  }
  return 0;
}

int
Tun::initialize(ErrorHandler *errh)
{
  _fd = alloc_tun(_dev_name.cc(), _near, _mask, errh);
  if (_fd < 0)
    return -1;
  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  add_select(_fd, SELECT_READ);
  return 0;
}

void
Tun::uninitialize()
{
  unschedule();
  if (_fd >= 0) {
    close(_fd);
    remove_select(_fd, SELECT_READ);
  }

  dealloc_tun();
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
  /*
   * ethertap driver is very picky about what address we use here.
   * e.g. if we have the wrong address, linux might ignore all the
   * packets, or accept udp or icmp, but ignore tcp.  aaarrrgh, well
   * this works.  -ddc */
  char to[] = { 0xfe, 0xfd, 0x0, 0x0, 0x0, 0x0 }; 
  char *from = to;
  short protocol = htons(0x0800);
  if(p->length()+16 >= sizeof(big)){
    fprintf(stderr, "bimtun writetun pkt too big\n");
    return;
  }
  memset(big, 0, 16);
  memcpy(big+2, from, sizeof(from)); // linux won't accept ethertap packets from eth addr 0.
  memcpy(big+8, to, sizeof(to)); // linux TCP doesn't like packets to 0??
  memcpy(big+14, &protocol, 2);
  memcpy(big+16, p->data(), p->length());
  if (write(_fd, big, p->length()+16) != (int)p->length()+16){
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
Tun::alloc_tun(const char *dev_prefix, struct in_addr near, struct in_addr mask,
               ErrorHandler *errh)
{
  int fd, yes = 1;
  char tmp[512], tmp0[64], tmp1[64];;

  for (int i = 0; i < 32; i++) {
    sprintf(tmp, "/dev/%s%d", dev_prefix, i);
    fd = open(tmp, 2);
    if(fd >= 0){
      if(ioctl(fd, FIONBIO, &yes) < 0){
	close(fd);
	return errh->error("FIONBIO failed");
      }

      _dev_name += String(i);
      
#if defined(TUNSIFMODE) || defined(__FreeBSD__)
      {
	int mode = IFF_BROADCAST;
	if(ioctl(fd, TUNSIFMODE, &mode) != 0){
	  perror("Tun: TUNSIFMODE");
	  return errh->error("cannot set TUNSIFMODE");
	}
      }
#endif
      
      strcpy(tmp0, inet_ntoa(near));
      strcpy(tmp1, inet_ntoa(mask));
      
      sprintf(tmp, "ifconfig %s %s netmask %s up", _dev_name.cc(), tmp0, tmp1);
      
      if(system(tmp) != 0){
	close(fd);
	return errh->error("failed: %s", tmp);
      }

      if (_gw) {
#if defined(__linux__)
	sprintf(tmp, "route -n add default gw %s", _gw.s().cc());
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	sprintf(tmp, "route -n add default %s", _gw.s().cc());
#else
#error Not supported on this system
#endif
	if (system(tmp) != 0){
	  close(fd);
	  return errh->error("failed: %s", tmp);
	}
      }

      return(fd);
    }
  }

  return errh->error("could not allocate a /dev/%s* device", dev_prefix);
}


void
Tun::dealloc_tun()
{
  String cmd = "ifconfig " + _dev_name + " down";
  if (system(cmd.cc()) != 0) 
    click_chatter("%s: failed: %s", id().cc(), cmd.cc());
}


ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(Tun)
