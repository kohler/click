#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "tobpf.hh"
#include "error.hh"
#include "etheraddress.hh"
#include "confparse.hh"
#include "router.hh"
#include "frombpf.hh"

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#ifdef __FreeBSD__
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif

ToBPF::ToBPF()
  : Element(1, 0), _fd(-1), _pcap(0)
{
}

ToBPF::ToBPF(const String &ifname)
  : Element(1, 0), _ifname(ifname), _fd(-1), _pcap(0)
{
}

ToBPF::~ToBPF()
{
  if (_pcap) pcap_close(_pcap);
}

ToBPF *
ToBPF::clone() const
{
  return new ToBPF(_ifname);
}

int
ToBPF::configure(const String &conf, ErrorHandler *errh)
{
  if (_pcap) pcap_close(_pcap);
  _pcap = 0;
  return cp_va_parse(conf, this, errh,
		     cpString, "interface name", &_ifname,
		     0);
}

int
ToBPF::initialize(ErrorHandler *errh)
{
#ifdef __FreeBSD__
  /* FreeBSD pcap_open_live() doesn't open for writing. */
  if(_fd >= 0)
    return(0);
  else if(!_ifname)
    return errh->error("interface not set");

  int i;
  for(i = 0; i < 16; i++){
    char tmp[64];
    sprintf(tmp, "/dev/bpf%d", i);
    int fd = open(tmp, 1);
    if(fd >= 0){
      _fd = fd;
      break;
    }
  }
  if(_fd < 0)
    return(errh->error("ToBPF: can't open a bpf"));

  struct ifreq ifr;
  (void)strncpy(ifr.ifr_name, _ifname.mutable_c_str(), sizeof(ifr.ifr_name));
  if (ioctl(_fd, BIOCSETIF, (caddr_t)&ifr) < 0)
    return(errh->error("ToBPF: BIOCSETIF %s failed", ifr.ifr_name));
  return(0);
#else
  if (_pcap || _fd >= 0)
    return 0;
  else if (!_ifname)
    return errh->error("interface not set");

  /*
   * Try to find a FromBPF with the same device and re-use its _pcap.
   * If we don't, Linux will give ToBPF's packets to FromBPF.
   */
  for(int fi = 0; fi < router()->nelements(); fi++){
    Element *f = router()->element(fi);
    FromBPF *lr = (FromBPF *)f->is_a_cast("FromBPF");
    if(lr && lr->get_ifname() == _ifname && lr->get_pcap()){
      _fd = pcap_fileno(lr->get_pcap());
      click_chatter("ToBPF(%s) borrowing FromBPF's pcap_fileno()", _ifname.cc());
    }
  }
  
  if(_fd < 0){
    char ebuf[PCAP_ERRBUF_SIZE];
    _pcap = pcap_open_live(_ifname.mutable_c_str(),
                           12000, /* XXX snaplen */
                           0,     /* not promiscuous */
                           0,     /* don't batch packets */
                           ebuf);
    if (!_pcap)
      return errh->error("%s: %s", _ifname.cc(), ebuf);
    _fd = pcap_fileno(_pcap);
  }
  return 0;
#endif
}

extern "C" {
extern int pcap_inject(pcap_t *p, const void *buf, size_t len);
extern int errno;
}

void
ToBPF::push(int, Packet *p)
{
  assert(p->length() >= 14);

#ifdef __linux__
  sockaddr sa;
  sa.sa_family = AF_INET;
  strcpy(sa.sa_data, _ifname);
  if (sendto(_fd, p->data(), p->length(),
	     0, &sa, sizeof(sa)) < 0) {
    perror("ToBPF: sendto to pcap");
  }
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
  int ret = write(_fd, p->data(), p->length());
  if(ret <= 0){
    fprintf(stderr, "ToBPF: write %d to _fd %d ret %d errno %d : %s\n",
            p->length(), _fd, ret, errno, strerror(errno));
  }
#endif

  p->kill();
}

bool
ToBPF::wants_packet_upstream() const
{
  return input_is_pull(0);
}

void
ToBPF::run_scheduled()
{
  while (Packet *p = input(0).pull())
    push(0, p);
}

EXPORT_ELEMENT(ToBPF)
ELEMENT_REQUIRES(FromBPF)
