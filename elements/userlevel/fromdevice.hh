#ifndef FROMDEVICE_HH
#define FROMDEVICE_HH

/*
=title FromDevice.u

=c

FromDevice(DEVNAME [, PROMISC, SNAPLEN, I<KEYWORDS>])

=s devices

reads packets from network device (user-level)

=d

This manual page describes the user-level version of the FromDevice
element. For the Linux kernel module element, read the FromDevice(n) manual
page.

Reads packets from the kernel that were received on the network controller
named DEVNAME. Puts the device in promiscuous mode if PROMISC (a Boolean) is
true. PROMISC defaults to false. On some systems, packets larger than SNAPLEN
will be truncated; default SNAPLEN is 2046 bytes.

The kernel networking code sees all of the packets that FromDevice
produces; be careful that at most one of Click and the kernel forwards each
packet.

Under Linux, a FromDevice element will not receive packets sent by a
ToDevice element for the same device. Under other operating systems, your
mileage may vary.

Sets the packet type annotation appropriately. Also sets the timestamp
annotation to the time the kernel reports that the packet was received.

Keyword arguments are:

=over 8

=item PROMISC

Boolean. Same as the PROMISC argument.

=item SNAPLEN

Unsigned integer. Same as the SNAPLEN argument.

=item FORCE_IP

Boolean. If true, then output only IP packets, with Ethernet header removed.
Default is false.

=item BPF_FILTER

String. A BPF filter expression used to select the interesting packets.
Default is the empty string, which means all packets. If FromDevice is not
using the pcap library to read its packets, any filter expression is ignored.

=back

=e

  FromDevice(eth0) -> ...

=n

FromDevice sets packets' extra length annotations as appropriate.

=a ToDevice.u, FromDump, ToDump, FromDevice(n) */

#include <click/element.hh>

#ifdef __linux__
# define FROMDEVICE_LINUX 1
#elif defined(HAVE_PCAP)
# define FROMDEVICE_PCAP 1
extern "C" {
# include <pcap.h>
}
#endif

class FromDevice : public Element { public:

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
#endif

  void selected(int fd);

#if FROMDEVICE_LINUX
  static int open_packet_socket(String, ErrorHandler *);
  static int set_promiscuous(int, String, bool);
#endif
  
 private:
  
#if FROMDEVICE_LINUX
  int _fd;
  unsigned char *_packetbuf;
#endif
#if FROMDEVICE_PCAP
  pcap_t* _pcap;
  static void get_packet(u_char *, const struct pcap_pkthdr *,
			 const u_char *);
#endif
  bool _force_ip;
  
  String _ifname;
  bool _promisc : 1;
  int _was_promisc : 2;
  int _packetbuf_size;
#if FROMDEVICE_PCAP
  String _bpf_filter;
#endif

  static bool check_force_ip(Packet *);

};

#endif
