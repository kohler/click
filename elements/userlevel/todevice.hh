#ifndef CLICK_TODEVICE_USERLEVEL_HH
#define CLICK_TODEVICE_USERLEVEL_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include "elements/userlevel/fromdevice.hh"
CLICK_DECLS

/*
 * =title ToDevice.u
 * =c
 * ToDevice(DEVNAME [, I<keywords>])
 * =s netdevices
 * sends packets to network device (user-level)
 * =d
 *
 * This manual page describes the user-level version of the ToDevice element.
 * For the Linux kernel module element, read the ToDevice(n) manual page.
 *
 * Pulls packets and sends them out the named device using
 * Berkeley Packet Filters (or Linux equivalent).
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item BURST
 *
 * Integer. Maximum number of packets to pull per scheduling. Defaults to 1.
 *
 * =item METHOD
 *
 * Word. Defines the method ToDevice will use to write packets to the
 * device. Linux targets generally support PCAP and LINUX; other targets
 * support PCAP or, occasionally, other methods. Generally defaults to PCAP.
 *
 * =item DEBUG
 *
 * Boolean.  If true, print out debug messages.
 *
 * =back
 *
 * This element is only available at user level.
 *
 * =n
 *
 * Packets sent via ToDevice should already have a link-level
 * header prepended. This means that ARP processing,
 * for example, must already have been done.
 *
 * The L<FromDevice(n)> element's OUTBOUND keyword argument determines whether
 * FromDevice receives packets sent by a ToDevice element for the same
 * device.
 *
 * Packets that are written successfully are sent on output 0, if it exists.
 * Packets that fail to be written are pushed out output 1, if it exists.

 * KernelTun lets you send IP packets to the host kernel's IP processing code,
 * sort of like the kernel module's ToHost element.
 *
 * =a
 * FromDevice.u, FromDump, ToDump, KernelTun, ToDevice(n) */

#if defined(__linux__)
# define TODEVICE_ALLOW_LINUX 1
#endif
#if HAVE_PCAP && (HAVE_PCAP_INJECT || HAVE_PCAP_SENDPACKET)
extern "C" {
# include <pcap.h>
}
# define TODEVICE_ALLOW_PCAP 1
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__) || defined(__NetBSD__)
# define TODEVICE_ALLOW_DEVBPF 1
#elif defined(__sun)
# define TODEVICE_ALLOW_PCAPFD 1
#endif
#if FROMDEVICE_ALLOW_NETMAP
# define TODEVICE_ALLOW_NETMAP 1
#endif

class ToDevice : public Element { public:

#if TODEVICE_ALLOW_NETMAP
    typedef struct FromDevice::netmap_type netmap_type;
#endif

    ToDevice();
    ~ToDevice();

    const char *class_name() const		{ return "ToDevice"; }
    const char *port_count() const		{ return "1/0-2"; }
    const char *processing() const		{ return "l/h"; }
    const char *flags() const			{ return "S2"; }

    int configure_phase() const { return KernelFilter::CONFIGURE_PHASE_TODEVICE; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    String ifname() const			{ return _ifname; }
    int fd() const				{ return _fd; }

    bool run_task(Task *);
    void selected(int fd, int mask);

  protected:

    Task _task;
    Timer _timer;

    String _ifname;
#if TODEVICE_ALLOW_PCAP
    pcap_t *_pcap;
#endif
#if TODEVICE_ALLOW_LINUX || TODEVICE_ALLOW_DEVBPF || TODEVICE_ALLOW_PCAPFD || TODEVICE_ALLOW_NETMAP
    int _fd;
#endif
#if TODEVICE_ALLOW_NETMAP
    netmap_type _netmap;
    int netmap_send_packet(Packet *p);
#endif
    enum { method_default, method_netmap, method_linux, method_pcap, method_devbpf, method_pcapfd };
    int _method;
    NotifierSignal _signal;

    Packet *_q;
    int _burst;

    bool _debug;
#if TODEVICE_ALLOW_PCAP
    bool _my_pcap;
#endif
#if TODEVICE_ALLOW_LINUX || TODEVICE_ALLOW_DEVBPF || TODEVICE_ALLOW_PCAPFD || TODEVICE_ALLOW_NETMAP
    bool _my_fd;
#endif
    int _backoff;
    int _pulls;

    enum { h_debug, h_signal, h_pulls, h_q };
    FromDevice *find_fromdevice() const;
    int send_packet(Packet *p);
    static int write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh);
    static String read_param(Element *e, void *thunk);

};

CLICK_ENDDECLS
#endif
