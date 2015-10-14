#ifndef CLICK_DPDKINFO_HH
#define CLICK_DPDKINFO_HH

#include <click/element.hh>
#include <click/dpdkdevice.hh>

CLICK_DECLS

/*
=title DpdkInfo

=c

DpdkInfo()

=s netdevices

Set DPDK global parameters.

=d

Set Intel's DPDK global parameters used by FromDpdkDevice and
ToDpdkDevice elements. See DPDK documentation for details.

Arguments:

=over 8

=item NB_MBUF

Integer.  Number of message buffers to allocate. Defaults to 524288.

=item MBUF_SIZE

Integer.  Size of a message buffer in bytes. Defaults to 2048 +
RTE_PKTMBUF_HEADROOM + sizeof (struct rte_mbuf).

=item MBUF_CACHE_SIZE

Integer.  Number of message buffer to keep in a per-core cache. It should be
such that NB_MBUF modulo MBUF_CACHE_SIZE == 0. Defaults to 256.

=item RX_PTHRESH

Integer.  RX prefetch threshold. Defaults to 8.

=item RX_HTHRESH

Integer.  RX host threshold. Defaults to 8.

=item RX_WTHRESH

Integer.  RX write-back threshold. Defaults to 4.

=item TX_PTHRESH

Integer.  TX prefetch threshold. Defaults to 36.

=item TX_HTHRESH

Integer.  TX host threshold. Defaults to 0.

=item TX_WTHRESH

Integer.  TX write-back threshold. Defaults to 0.

=back

This element is only available at user level, when compiled with DPDK
support.

=e

  DpdkInfo(NB_MBUF 1048576, MBUF_SIZE 4096, MBUF_CACHE_SIZE 512)

=a FromDpdkDevice, ToDpdkDevice */

class DpdkInfo : public Element {
public:

    const char *class_name() const { return "DpdkInfo"; }

    int configure_phase() const { return CONFIGURE_PHASE_FIRST; }

    int configure(Vector<String> &conf, ErrorHandler *errh);

    static DpdkInfo *instance;
};

CLICK_ENDDECLS

#endif
