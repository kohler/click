// -*- c-basic-offset: 4 -*-
#ifndef CLICK_KERNELTUN_HH
#define CLICK_KERNELTUN_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =c
 *
 * KernelTun(ADDR/MASK [, GATEWAY, I<keywords> HEADROOM, MTU, IGNORE_QUEUE_OVERFLOWS])
 *
 * =s devices
 *
 * user-level interface to /dev/tun or ethertap
 *
 * =d
 *
 * Reads IP packets from and writes IP packets to a /dev/net/tun, /dev/tun*,
 * or /dev/tap* device. This allows a user-level Click to hand packets to the
 * ordinary kernel IP processing code. KernelTun will also install a routing
 * table entry so that the kernel can pass packets to the KernelTun device.
 *
 * KernelTun produces and expects IP packets. If, for some reason, the kernel
 * passes up a non-IP packet (or an invalid IP packet), KernelTun will emit
 * that packet on its second output, or drop it if there is no second output.
 *
 * KernelTun allocates a /dev/net/tun, /dev/tun*, or /dev/tap* device (this
 * might fail) and runs ifconfig(8) to set the interface's local (i.e.,
 * kernel) address to ADDR and the netmask to MASK. If a nonzero GATEWAY IP
 * address (which must be on the same network as the tun) is specified, then
 * KernelTun tries to set up a default route through that host.
 *
 * When cleaning up, KernelTun attempts to bring down the device via
 * ifconfig(8).
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item HEADROOM
 *
 * Integer. The number of bytes left empty before the packet data to leave
 * room for additional encapsulation headers. Default is 28.
 *
 * =item MTU
 *
 * Integer. The interface's MTU. KernelTun will refuse to send packets larger
 * than the MTU. Default is 2048.
 *
 * =item IGNORE_QUEUE_OVERFLOWS
 *
 * Boolean.  If true, don't print more than one error message when
 * there are queue overflows error when sending/receiving packets
 * to/from the tun device (e.g. there was an ENOBUFS error).  Default
 * is false.
 *
 * =back
 *
 * =n
 *
 * This element replaces the KernelTap element, which generated and required
 * Ethernet headers in addition to IP headers.
 *
 * Make sure that your kernel has tun support enabled before running
 * KernelTun. Initialization errors like "no such device" or "no such file or
 * directory" may indicate that your kernel isn't set up, or that some
 * required kernel module hasn't been loaded (on Linux, the relevant module is
 * "tun").
 *
 * This element differs from KernelTap in that it produces and expects IP
 * packets, not IP-in-Ethernet packets.
 *
 * =a
 *
 * FromDevice.u, ToDevice.u, KernelTap, ifconfig(8) */

class KernelTun : public Element { public:
  
    KernelTun();
    ~KernelTun();
  
    const char *class_name() const	{ return "KernelTun"; }
    const char *processing() const	{ return "a/h"; }
    const char *flow_code() const	{ return "x/y"; }
    KernelTun *clone() const;
    const char *flags() const		{ return "S3"; }

    void notify_ninputs(int);
    void notify_noutputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void selected(int fd);

    void push(int port, Packet *);
    bool run_task();

  private:

    enum { DEFAULT_MTU = 2048 };
    enum Type { LINUX_UNIVERSAL, LINUX_ETHERTAP, BSD_TUN };

    int _fd;
    int _mtu_in;
    int _mtu_out;
    Type _type;
    String _dev_name;
    IPAddress _near;
    IPAddress _mask;
    IPAddress _gw;
    int _headroom;
    Task _task;
    
    bool _ignore_q_errs;
    bool _printed_write_err;
    bool _printed_read_err;

    static String print_dev_name(Element *e, void *);

#ifdef HAVE_LINUX_IF_TUN_H
    int try_linux_universal(ErrorHandler *);
#endif
    int try_tun(const String &, ErrorHandler *);
    int alloc_tun(ErrorHandler *);
    int setup_tun(struct in_addr near, struct in_addr mask, ErrorHandler *);
    void dealloc_tun();
    
};

CLICK_ENDDECLS
#endif
