// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FROMHOST_HH
#define CLICK_FROMHOST_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =title FromDevice.u
 * 
 * =c
 * 
 * FromHost(DEVNAME, ADDR/MASK [, GATEWAY, HEADROOM] [, I<KEYWORDS>])
 * 
 * =s devices
 *
 *user-level interface to /dev/tap or ethertap
 *
 *=d
 *
 * Reads packets from and writes packets through the universal TUN/TAP
 * module in linux (the /dev/net/tun device).  This allows a
 * user-level Click to hand packets to the virtual ethernet
 * device. FromHost will also transfer packets from the virtual
 * ethernet device.
 *
 * To use this element you must have 
 * CONFIG_TUN
 * CONFIG_ETHERTAP
 * included in your kernel config (ie. the tun.o module is available).
 * Either modules or compiled in should work.
 *
 *
 * FromHost allocates a /dev/net/tun device (this might fail) and
 * runs ifconfig(8) to set the interface's local (i.e., kernel)
 * address to ADDR and the netmask to MASK. If a nonzero GATEWAY IP
 * address (which must be on the same network as the tun) is
 * specified, then FromHost tries to set up a default route through
 * that host.  HEADROOM is the number of bytes left empty before the
 * packet data (to leave room for additional encapsulation
 * headers). Default HEADROOM is 0.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item ETHER
 *
 * Ethernet address. Specifies the fake device's Ethernet address. Default is
 * 00:a01:02:03:04:05.
 *
 * =back
 *
 * =n
 *
 * Linux will send ARP queries to the fake device. You must respond to these
 * queries in order to receive any IP packets, but you can obviously respond
 * with any Ethernet address you'd like. Here is one common idiom:
 *
 * tap0 :: FromHost(192.0.0.1/8)
 * -> fromhost_cl :: Classifier(12/0806, 12/0800);
 * fromhost_cl[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> tap0;
 * fromhost_cl[1] -> ... // IP packets
 *  
 *  =e
 *
 *  FromHost(192.0.0.1/8) -> ...;
 * =n
 * 
 * An error like "could not allocate a /dev/tap* device : No such file or
 * directory" usually means that you have not enabled /dev/tap* in your
 * kernel. 
 * 
 * =a ToLinux, ifconfig(8) */

class FromHost : public Element { public:


    enum ConfigurePhase {
	CONFIGURE_PHASE_FROMHOST = CONFIGURE_PHASE_DEFAULT,
	CONFIGURE_PHASE_TOHOST =  CONFIGURE_PHASE_FROMHOST + 1
    };

    FromHost();
    ~FromHost();
  
    const char *class_name() const	{ return "FromHost"; }
    const char *processing() const	{ return PUSH; }
  

    int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void selected(int fd);

    int fd() { return _fd; }
    String dev_name() { return _dev_name; }

  private:
    enum { DEFAULT_MTU = 2048 };

    int _fd;
    int _mtu_in;
    int _mtu_out;
    String _dev_name;
    IPAddress _near;
    IPAddress _mask;
    int _headroom;

    EtherAddress _macaddr;

    bool _ignore_q_errs;
    bool _printed_write_err;
    bool _printed_read_err;

    static String print_dev_name(Element *e, void *);

    int try_linux_universal(ErrorHandler *);
    int try_tun(const String &, ErrorHandler *);
    int alloc_tun(ErrorHandler *);
    int setup_tun(struct in_addr near, struct in_addr mask, ErrorHandler *);
    void dealloc_tun();
    
};

CLICK_ENDDECLS
#endif
