#ifndef FROMDEVICE_HH
#define FROMDEVICE_HH

/*
 * =title FromDevice.u
 * =c
 * FromDevice(DEVNAME [, PROMISC? [, MAXPACKETSIZE]])
 * =s reads packets from network device (user-level)
 * =d
 *
 * This manual page describes the user-level version of the FromDevice
 * element. For the Linux kernel module element, read the FromDevice(n) manual
 * page.
 *
 * Reads packets from the kernel that were received on the network controller
 * named DEVNAME. Puts the device in promiscuous mode if PROMISC? (a Boolean)
 * is true. PROMISC? defaults to false. On some systems, packets larger than
 * MAXPACKETSIZE will be truncated; default MAXPACKETSIZE is 2048 bytes.
 *
 * The kernel networking code sees all of the packets that FromDevice
 * produces; be careful that at most one of Click and the kernel forwards each
 * packet.
 *
 * Under Linux, a FromDevice element will not receive packets sent by a
 * ToDevice element for the same device. Under other operating systems, your
 * mileage may vary.
 *
 * =e
 *   FromDevice(eth0, 0) -> ...
 *
 * =a ToDevice.u, FromDump, ToDump, FromDevice(n) */

#include "element.hh"

#ifdef __linux__
# define FROMDEVICE_LINUX 1
#elif defined(HAVE_PCAP)
# define FROMDEVICE_PCAP 1
extern "C" {
# include <pcap.h>
}
#endif

class FromDevice : public Element {

  String _ifname;
  bool _promisc : 1;
  int _was_promisc : 2;
  int _packetbuf_size;

#if FROMDEVICE_LINUX
  int _fd;
  unsigned char *_packetbuf;
#endif
#if FROMDEVICE_PCAP
  pcap_t* _pcap;
  static void get_packet(u_char *, const struct pcap_pkthdr *,
			 const u_char *);
  int do_select(int waitms);
#endif

 public:

  enum ConfigurePhase {
    CONFIGURE_PHASE_FROMDEVICE = CONFIGURE_PHASE_DEFAULT,
    CONFIGURE_PHASE_TODEVICE = CONFIGURE_PHASE_FROMDEVICE + 1
  };
  
  FromDevice();
  ~FromDevice();
  
  const char *class_name() const	{ return "FromDevice"; }
  const char *processing() const	{ return PUSH; }
  
  FromDevice *clone() const;
  int configure_phase() const		{ return CONFIGURE_PHASE_FROMDEVICE; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  String ifname() const			{ return _ifname; }
#if FROMDEVICE_PCAP
  pcap_t *pcap() const			{ return _pcap; }  
#endif
#if FROMDEVICE_LINUX
  int fd() const			{ return _fd; }
  void selected(int fd);
#endif

  void run_scheduled();

#if FROMDEVICE_LINUX
  static int open_packet_socket(String, ErrorHandler *);
  static int set_promiscuous(int, String, bool);
#endif
  
};

#endif
