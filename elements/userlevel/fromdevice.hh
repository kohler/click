#ifndef CLICK_FROMDEVICE_HH
#define CLICK_FROMDEVICE_HH
#include <click/element.hh>
#ifdef __linux__
# define FROMDEVICE_LINUX 1
#endif
#ifdef HAVE_PCAP
# define FROMDEVICE_PCAP 1
# include <click/task.hh>
extern "C" {
# include <pcap.h>
/* Prototype pcap_setnonblock if we have it, but not the prototype. */
# if HAVE_PCAP_SETNONBLOCK && !HAVE_DECL_PCAP_SETNONBLOCK
int pcap_setnonblock(pcap_t *p, int nonblock, char *errbuf);
# endif
void FromDevice_get_packet(u_char*, const struct pcap_pkthdr*, const u_char*);
}
#endif
CLICK_DECLS

/*
=title FromDevice.u

=c

FromDevice(DEVNAME [, I<keywords> SNIFFER, PROMISC, SNAPLEN, FORCE_IP, CAPTURE, BPF_FILTER, OUTBOUND])

=s netdevices

reads packets from network device (user-level)

=d

This manual page describes the user-level version of the FromDevice
element. For the Linux kernel module element, read the FromDevice(n) manual
page.

Reads packets from the kernel that were received on the network controller
named DEVNAME.

User-level FromDevice behaves like a packet sniffer by default.  Packets
emitted by FromDevice are also received and processed by the kernel.  Thus, it
doesn't usually make sense to run a router with user-level Click, since each
packet will get processed twice (once by Click, once by the kernel).  Install
firewalling rules in your kernel if you want to prevent this, for instance
using the KernelFilter element or FromDevice's SNIFFER false argument.

Under Linux, a FromDevice element will not receive packets sent by a
ToDevice element for the same device. Under other operating systems, your
mileage may vary.

Sets the packet type annotation appropriately. Also sets the timestamp
annotation to the time the kernel reports that the packet was received.

Keyword arguments are:

=over 8

=item SNIFFER

Boolean.  Specifies whether FromDevice should run in sniffer mode.  In
non-sniffer mode, FromDevice installs KernelFilter filtering rules to block
the kernel from handling any packets arriving on device DEVNAME.  Default is
true (sniffer mode).

=item PROMISC

Boolean.  FromDevice puts the device in promiscuous mode if PROMISC is true.
The default is false.

=item SNAPLEN

Unsigned.  On some systems, packets larger than SNAPLEN will be truncated.
Defaults to 2046.

=item FORCE_IP

Boolean. If true, then output only IP packets. (Any link-level header remains,
but the IP header annotation has been set appropriately.) Default is false.

=item CAPTURE

Word.  Defines the capture method FromDevice will use to read packets from the
kernel.  Linux targets generally support PCAP and LINUX; other targets support
only PCAP.  Defaults to LINUX on Linux targets (unless you give a BPF_FILTER),
and PCAP elsewhere.

=item BPF_FILTER

String.  A BPF filter expression used to select the interesting packets.
Default is the empty string, which means all packets.  If CAPTURE is not PCAP,
then any filter expression is ignored with a warning.

=item OUTBOUND

Boolean. If true, then emit packets that the kernel sends to the given
interface, as well as packets that the kernel receives from it. Default is
false.

=back

=e

  FromDevice(eth0) -> ...

=n

FromDevice sets packets' extra length annotations as appropriate.

=h count read-only

Returns the number of packets read by the device.

=h reset_counts write-only

Resets "count" to zero.

=h kernel_drops read-only

Returns the number of packets dropped by the kernel, probably due to memory
constraints, before FromDevice could get them. This may be an integer; the
notation C<"<I<d>">, meaning at most C<I<d>> drops; or C<"??">, meaning the
number of drops is not known.

=h encap read-only

Returns a string indicating the encapsulation type on this link. Can be
`C<IP>', `C<ETHER>', or `C<FDDI>', for example.

=a ToDevice.u, FromDump, ToDump, KernelFilter, FromDevice(n) */

class FromDevice : public Element { public:

    enum ConfigurePhase {
	CONFIGURE_PHASE_FROMDEVICE = CONFIGURE_PHASE_PRIVILEGED - 1,
	CONFIGURE_PHASE_TODEVICE = CONFIGURE_PHASE_PRIVILEGED
    };
  
    FromDevice();
    ~FromDevice();
  
    const char *class_name() const	{ return "FromDevice"; }
    const char *port_count() const	{ return PORTS_0_1; }
    const char *processing() const	{ return PUSH; }
  
    int configure_phase() const		{ return CONFIGURE_PHASE_FROMDEVICE; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();
  
    String ifname() const		{ return _ifname; }
    inline int fd() const;

    void selected(int fd);
#if FROMDEVICE_PCAP
    bool run_task(Task *);
#endif

#if FROMDEVICE_LINUX
    static int open_packet_socket(String, ErrorHandler *);
    static int set_promiscuous(int, String, bool);
#endif

    void kernel_drops(bool& known, int& max_drops) const;
  
  private:
  
#if FROMDEVICE_LINUX
    int _linux_fd;
    unsigned char *_linux_packetbuf;
#endif
#if FROMDEVICE_PCAP
    pcap_t* _pcap;
    Task _pcap_task;
    int _pcap_complaints;
    friend void FromDevice_get_packet(u_char*, const struct pcap_pkthdr*,
				      const u_char*);
#endif
    bool _force_ip;
    int _datalink;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;
  
    String _ifname;
    bool _sniffer : 1;
    bool _promisc : 1;
    bool _outbound : 1;
    int _was_promisc : 2;
    int _snaplen;
    enum { CAPTURE_PCAP, CAPTURE_LINUX };
    int _capture;
#if FROMDEVICE_PCAP
    String _bpf_filter;
#endif

    static String read_handler(Element*, void*);
    static int write_handler(const String&, Element*, void*, ErrorHandler*);

};


inline int
FromDevice::fd() const
{
#if FROMDEVICE_LINUX
    if (_linux_fd >= 0)
	return _linux_fd;
#endif
#if FROMDEVICE_PCAP
    if (_pcap)
	return pcap_fileno(_pcap);
#endif
    return -1;
}

CLICK_ENDDECLS
#endif
