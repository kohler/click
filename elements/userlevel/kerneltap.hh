// -*- c-basic-offset: 4 -*-
#ifndef CLICK_KERNELTAP_HH
#define CLICK_KERNELTAP_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =c
 *
 * KernelTap(ADDR/MASK [, GATEWAY, HEADROOM] [, KEYWORDS])
 *
 * =s devices
 *
 * user-level interface to /dev/tun or ethertap
 *
 * =deprecated KernelTun
 *
 * =d
 *
 * This element is deprecated. Use KernelTun instead.
 *
 * Reads packets from and writes packets to a /dev/tun* or /dev/tap* device.
 * This allows a user-level Click to hand packets to the ordinary kernel IP
 * processing code. KernelTap will also transfer packets from the kernel IP
 * code if the kernel routing table has entries pointing at the device.
 *
 * Much like ToLinux, KernelTap produces and expects Ethernet packets, but the
 * Ethernet source and destination addresses are meaningless; only the
 * protocol usually matters.
 *
 * KernelTap allocates a /dev/tun* or tap* device (this might fail) and runs
 * ifconfig(8) to set the interface's local (i.e., kernel) address to ADDR and
 * the netmask to MASK. If a nonzero GATEWAY IP address (which must be on the
 * same network as the tun) is specified, then KernelTap tries to set up a
 * default route through that host.
 *
 * When cleaning up, KernelTap attempts to bring down the device via
 * ifconfig(8).
 *
 * HEADROOM is the number of bytes left empty before the packet data (to leave
 * room for additional encapsulation headers). Default HEADROOM is 0.
 *
 * Keyword arguments are:
 *
 * =over 8
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
 * An error like "could not allocate a /dev/tap* device : No such file or
 * directory" usually means that you have not enabled /dev/tap* in your
 * kernel. 
 *
 * =a
 * ToLinux, ifconfig(8) */

class KernelTap : public Element { public:
  
    KernelTap();
    ~KernelTap();
  
    const char *class_name() const	{ return "KernelTap"; }
    const char *processing() const	{ return "a/h"; }
    const char *flow_code() const	{ return "x/y"; }
    KernelTap *clone() const;
    const char *flags() const		{ return "S3"; }
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void selected(int fd);

    void push(int port, Packet *);
    bool run_task();

  private:

    enum Type { LINUX_UNIVERSAL, LINUXBSD_TUN };

    int _fd;
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

#if HAVE_LINUX_IF_TUN_H
    int try_linux_universal(ErrorHandler *);
#endif
    int try_tun(const String &, ErrorHandler *);
    int alloc_tun(ErrorHandler *);
    int setup_tun(struct in_addr near, struct in_addr mask, ErrorHandler *);
    void dealloc_tun();
    
};

CLICK_ENDDECLS
#endif
