#ifndef CLICK_DPDKINFO_HH
#define CLICK_DPDKINFO_HH

#include <click/element.hh>
#include <click/dpdkdevice.hh>

CLICK_DECLS

/*
=title DPDKInfo

=c

DPDKInfo([I<keywords> NB_MBUF, MBUF_SIZE, RX_PTHRESH, etc.])

=s netdevices

Set DPDK global parameters.

=d

Set Intel's DPDK global parameters used by FromDPDKDevice and
ToDPDKDevice elements. See DPDK documentation for details.

Keyword arguments:

=over 8

=item NB_MBUF

Integer.  Number of message buffers to allocate. Defaults to 65536.

=item MBUF_SIZE

Integer.  Size of a message buffer in bytes. Defaults to 2048 +
RTE_PKTMBUF_HEADROOM.

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

=item MEMPOOL_PREFIX

String. Prefix for the mempool name. Use this to get a predictable
  mempool name and attach secondary processes.

=item DEF_BURST_SIZE

Integer.  Number of frames to read/write from/to a DPDK device.
Defaults to 32.

=back

This element is only available at user level, when compiled with DPDK
support.

=e

  DPDKInfo(NB_MBUF 1048576, MBUF_SIZE 4096, MBUF_CACHE_SIZE 512)

=a FromDPDKDevice, ToDPDKDevice */

class DPDKInfo : public Element {
public:

    const char *class_name() const { return "DPDKInfo"; }

    int configure_phase() const { return CONFIGURE_PHASE_FIRST; }

    int configure(Vector<String> &conf, ErrorHandler *errh);

    enum {h_pool_count};
    static String read_handler(Element *e, void * thunk);
    void add_handlers() override;

    static DPDKInfo *instance;
};

CLICK_ENDDECLS

#endif
