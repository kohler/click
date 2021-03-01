// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FROMHOST_USERLEVEL_HH
#define CLICK_FROMHOST_USERLEVEL_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#if HAVE_IP6
# include <click/ip6address.hh>
#endif
CLICK_DECLS

/*
 * =title FromHost.u
 *
 * =c
 *
 * FromHost(DEVNAME [, DST] [, I<keywords> GATEWAY, HEADROOM, ...])
 *
 * =s comm
 *
 * interface to /dev/net/tun or ethertap (user-level)
 *
 * =d
 *
 * B<Note>: The KernelTun and KernelTap elements are preferable for most
 * purposes.
 *
 * Reads packets from and writes packets through the universal TUN/TAP
 * module in Linux (the /dev/net/tun device).  This allows a
 * user-level Click to hand packets to the virtual ethernet
 * device. FromHost will also transfer packets from the virtual
 * ethernet device.
 *
 * To use this element your kernel config must support CONFIG_TUN and
 * CONFIG_ETHERTAP.  Either modules (tun.o) or compiled in should work.
 *
 * FromHost allocates a /dev/net/tun device (this might fail) and runs
 * ifconfig(8) to set the interface's local (i.e., kernel) address and netmask
 * to DST, which must be an IP prefix such as 18.26.4.9/24.  If DST is not
 * specified, then FromHost
 * assumes the tunnel has already been configured to the correct address.  If
 * a nonzero GATEWAY IP address (which must be on the same network as the tun)
 * is specified, then FromHost tries to set up a default route through that
 * host.  HEADROOM is the number of bytes left empty before the packet data
 * (to leave room for additional encapsulation headers). Default HEADROOM is
 * roughly 28.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item ETHER
 *
 * Ethernet address. Specifies the fake device's Ethernet address. Default is
 * not specified, in which case the fake device's address is whatever the
 * kernel chooses.
 *
 * =item DST6
 *
 * IPv6 prefix.  If specified, FromHost runs ifconfig(8) to set the
 * interface's local (i.e., kernel) IPv6 address and netmask.  Both DST and
 * DST6 may be specified.
 *
 * =back
 *
 * =n
 *
 * Linux will send ARP queries to the fake device. You must respond to these
 * queries in order to receive any IP packets, but you can obviously respond
 * with any Ethernet address you'd like. Here is one common idiom:
 *
 *  FromHost(fake, 192.0.0.1/8)
 *      -> fromhost_cl :: Classifier(12/0806, 12/0800);
 *  fromhost_cl[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> ToHost(fake);
 *  fromhost_cl[1] -> ... // IP packets
 *
 * =e
 *
 *  FromHost(fake, 192.0.0.1/8) -> ...;
 *
 * An error like "open /dev/net/tun: No such file or directory" usually means
 * that you have not enabled tunnel support in your kernel.
 *
 * =h dev_name read-only
 * Returns the name of the device that this element is using.
 *
 * =a
 *
 * ToHost.u, ifconfig(8)
 *
 */

class FromHost : public Element {
public:
    enum ConfigurePhase {
	CONFIGURE_PHASE_FROMHOST = CONFIGURE_PHASE_DEFAULT,
	CONFIGURE_PHASE_TOHOST =  CONFIGURE_PHASE_FROMHOST + 1
    };

    FromHost() CLICK_COLD;
    ~FromHost() CLICK_COLD;

    const char *class_name() const	{ return "FromHost"; }
    const char *port_count() const	{ return PORTS_0_1; }
    const char *processing() const	{ return PUSH; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    FromHost *hotswap_element() const;
    void take_state(Element *, ErrorHandler *);

    int fd() const			{ return _fd; }
    String dev_name() const		{ return _dev_name; }

    void selected(int fd, int mask);
    bool run_task(Task *);

  private:
    enum { DEFAULT_MTU = 2048 };

    int _fd;
    int _mtu_in;
    int _mtu_out;
    String _dev_name;

    EtherAddress _macaddr;

    IPAddress _near;
    IPAddress _mask;
    IPAddress _gw;

#if HAVE_IP6
    IP6Address _near6;
    int _prefix6;
#endif

    unsigned _headroom;
    Task _task;
    NotifierSignal _nonfull_signal;

    int try_linux_universal(ErrorHandler *);
    int try_tun(const String &, ErrorHandler *);
    int alloc_tun(ErrorHandler *);
    int setup_tun(ErrorHandler *);
    void dealloc_tun();

    static String read_param(Element *, void *) CLICK_COLD;
};

CLICK_ENDDECLS
#endif
