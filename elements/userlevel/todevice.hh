#ifndef TODEVICE_HH
#define TODEVICE_HH
#include "element.hh"
#include "string.hh"
#include "elements/userlevel/fromdevice.hh"

/*
 * =title ToDevice.u
 * =c
 * ToDevice(DEVNAME)
 * =s
 * sends packets to network device (user-level)
 * V<devices>
 * =d
 *
 * This manual page describes the user-level version of the ToDevice element.
 * For the Linux kernel module element, read the ToDevice(n) manual page.
 *
 * Pulls packets and sends them out the named device using
 * Berkeley Packet Filters (or Linux equivalent).
 *
 * Packets sent via ToDevice should already have a link-level
 * header prepended. This means that ARP processing,
 * for example, must already have been done.
 *
 * Under Linux, a L<FromDevice(n)> element will not receive packets sent by a
 * ToDevice element for the same device. Under other operating systems, your
 * mileage may vary.
 *
 * This element is only available at user level.
 * 
 * =a
 * FromDevice.u, FromDump, ToDump, ToDevice(n) */

#ifdef HAVE_PCAP
extern "C" {
# include <pcap.h>
}
#else
# include "fakepcap.h"
#endif

#ifdef HAVE_PCAP
# if defined(__FreeBSD__) || defined(__OpenBSD__)
#  define TODEVICE_BSD_DEV_BPF 1
#  define TODEVICE_WRITE 1
# endif
#endif
#if defined(__linux__)
# define TODEVICE_LINUX 1
# define TODEVICE_SEND 1
#endif

/*
 * Write packets to the ethernet via the bpf.
 * Expects packets that already have an ether header.
 * Can push or pull.
 */

class ToDevice : public Element {

  String _ifname;
  int _fd;
  bool _my_fd;
  
#if TODEVICE_BSD_DEV_BPF
  pcap_t *_pcap;
#endif

  void send_packet(Packet *);
  
 public:
  
  ToDevice();
  ~ToDevice();
  
  const char *class_name() const		{ return "ToDevice"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flags() const			{ return "S2"; }
  
  ToDevice *clone() const;
  int configure_phase() const { return FromDevice::CONFIGURE_PHASE_TODEVICE; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  String ifname() const				{ return _ifname; }
  int fd() const				{ return _fd; }

  void push(int port, Packet *);
  void run_scheduled();
  
};

#endif
