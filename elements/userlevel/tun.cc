#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "tun.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "glue.hh"
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
  if(_fd >= 0)
    close(_fd);
}

Tun *
Tun::clone() const
{
  return new Tun();
}

int
Tun::configure(const String &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpIPAddress, "near address", &_near,
		  cpIPAddress, "far address", &_far,
		  cpEnd) < 0)
    return -1;

  return 0;
}

int
Tun::initialize(ErrorHandler *errh)
{
  _fd = alloc_tun(_near, _far, errh);
  if(_fd < 0)
    return(-1);

  return 0;
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
#ifdef __OpenBSD__
    Packet *p = Packet::make(b+4, cc-4);
#else
    Packet *p = Packet::make(b, cc);
#endif
    output(0).push(p);
  } else {
    perror("Tun read");
  }
}

bool
Tun::wants_packet_upstream() const
{
  return input_is_pull(0);
}

void
Tun::run_scheduled()
{
  while (Packet *p = input(0).pull())
    push(0, p);
}

void
Tun::push(int, Packet *p)
{
#ifdef __OpenBSD__
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
Tun::alloc_tun(struct in_addr near, struct in_addr far,
               ErrorHandler *errh)
{
  int i, fd, yes = 1;
  char tmp[512], tmp0[64], tmp1[64];;

  for(i = 0; i < 32; i++){
    sprintf(tmp, "/dev/tun%d", i);
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
      sprintf(tmp, "ifconfig tun%d %s %s up", i, tmp0, tmp1);
      if(system(tmp) != 0){
        close(fd);
        return errh->error("failed: %s", tmp);
      }
      return(fd);
    }
  }

  return errh->error("could not allocate a free /dev/tun* device");
}

EXPORT_ELEMENT(Tun)
