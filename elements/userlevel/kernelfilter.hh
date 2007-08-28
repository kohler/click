// -*- c-basic-offset: 4 -*-
#ifndef CLICK_KERNELFILTER_HH
#define CLICK_KERNELFILTER_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

KernelFilter(FILTERSPEC, ...)

=s comm

block kernel from handling packets

=d

The KernelFilter element installs filter rules in the kernel to stop the
kernel from handling certain types of packets.  Use this in combination with
FromDevice.u to handle packets in user-level Click configurations.

KernelFilter uses iptables(1) to install filters; if your system does not
support iptables(1), KernelFilter will fail.

KernelFilter uninstalls its firewall rules when Click shuts down.  If Click
shuts down uncleanly, for instance because of a segmentation fault or 'kill
-9', then the rules will remain in place, and you'll have to remove them
yourself.

Currently only one form of FILTERSPEC is understood.

=over 8

=item 'C<drop dev DEVNAME>'

The kernel is blocked from handling any packets arriving on device DEVNAME.
However, these packets will still be visible to tcpdump(1), and to Click
elements like FromDevice.u.

=back

=a

FromDevice.u, ToDevice.u, KernelTap, ifconfig(8) */

class KernelFilter : public Element { public:
  
    KernelFilter();
    ~KernelFilter();
  
    const char *class_name() const	{ return "KernelFilter"; }
    const char *port_count() const	{ return PORTS_0_0; }
    int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    static int device_filter(const String &devname, bool add, ErrorHandler *);

  private:

    Vector<String> _filters;
    
};

CLICK_ENDDECLS
#endif
