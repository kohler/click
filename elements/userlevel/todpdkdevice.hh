#ifndef CLICK_TODPDKDEVICE_USERLEVEL_HH
#define CLICK_TODPDKDEVICE_USERLEVEL_HH

#include <click/element.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/dpdkdevice.hh>

CLICK_DECLS

/*
=title ToDPDKDevice

=c

ToDPDKDevice(PORT [, QUEUE [, I<keywords> IQUEUE, BLOCKING, etc.]])

=s netdevices

sends packets to network device using Intel's DPDK (user-level)

=d

Sends packets to a network device with DPDK port identifier PORT. As DPDK does
not support polling, this element only supports PUSH. It will build a batch of
packets inside an internal queue (limited to IQUEUE packets) until it reaches
BURST packets, and then send the batch to DPDK. If the batch is not ready after
TIMEOUT ms, it will flush the batch of packets even if it doesn't cointain
BURST packets.

Arguments:

=over 8

=item PORT

Integer or PCI address.  Port identifier of the device, or a PCI address in the
format fffff:ff:ff.f

=item QUEUE

Integer.  Index of the queue to use. If omitted or negative, auto-increment
between ToDPDKDevice attached to the same port will be used.

=item IQUEUE

Integer.  Size of the internal queue, i.e. number of packets that we can buffer
before pushing them to the DPDK framework. If IQUEUE is bigger than BURST,
some packets could be buffered in the internal queue when the output ring is
full. Defaults to 1024.

=item BLOCKING

Boolean.  If true, when there is no more space in the output device ring, and
the IQUEUE is full, we'll block until some packet could be sent. If false the
packet will be dropped. Defaults to true.

=item BURST

Integer.  Number of packets to batch before sending them out. A bigger BURST
leads to more latency, but a better throughput. The default value of 32 is
recommended as it is what DPDK will do under the hood. Prefer to set the
TIMEOUT parameter to 0 if the throughput is low as it will maintain
performance.

=item TIMEOUT

Integer.  Set a timeout to flush the internal queue. It is useful under low
throughput as it could take a long time before reaching BURST packet in the
internal queue. The timeout is expressed in milliseconds. Setting the timer to
0 is not a bad idea as it will schedule after the source element (such as a
FromDPDKDevice) will have finished its burst, or all incoming packets. This
would therefore ensure that a flush is done right after all packets have been
processed by the Click pipeline. Setting a negative value disable the timer,
this is generally acceptable if the thoughput of this element rarely drops
below 32000 pps (~50 Mbps with maximal size packets) with a BURST of 32, as the
internal queue will wait on average 1 ms before containing 32 packets. Defaults
to 0 (immediate flush).

=item NDESC

Integer.  Number of descriptors per ring. The default is 1024.

=back

This element is only available at user level, when compiled with DPDK support.

=e

  ... -> ToDPDKDevice(2, QUEUE 0, BLOCKING true)

=h n_sent read-only

Returns the number of packets sent by the device.

=h n_dropped read-only

Returns the number of packets dropped by the device.

=h reset_counts write-only

Resets n_send and n_dropped counts to zero.

=a DPDKInfo, FromDPDKDevice */

class ToDPDKDevice : public Element {
public:

    ToDPDKDevice() CLICK_COLD;
    ~ToDPDKDevice() CLICK_COLD;

    const char *class_name() const { return "ToDPDKDevice"; }
    const char *port_count() const { return PORTS_1_0; }
    const char *processing() const { return PUSH; }
    int configure_phase() const {
        return CONFIGURE_PHASE_PRIVILEGED;
    }
    bool can_live_reconfigure() const { return false; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;

    void cleanup(CleanupStage stage) CLICK_COLD;

    void add_handlers() CLICK_COLD;

    void run_timer(Timer *);
    void push(int port, Packet *p);

private:

    /* InternalQueue is a ring of DPDK buffers pointers (rte_mbuf *) awaiting
     * to be sent.
     * index is the index of the first valid packets awaiting to be sent, while
     * nr_pending is the number of packets. index + nr_pending may be greater
     * than _iqueue_size but index should be wrapped-around. */
    class InternalQueue {
    public:
        InternalQueue() : pkts(0), index(0), nr_pending(0) { }

        // Array of DPDK Buffers
        struct rte_mbuf ** pkts;
        // Index of the first valid packet in the pkts array
        unsigned int index;
        // Number of valid packets awaiting to be sent after index
        unsigned int nr_pending;

        // Timer to limit time a batch will take to be completed
        Timer timeout;
    } __attribute__((aligned(64)));

    static String n_sent_handler(Element *, void *) CLICK_COLD;
    static String n_dropped_handler(Element *, void *) CLICK_COLD;
    static int reset_counts_handler(const String &, Element *, void *,
                                    ErrorHandler *) CLICK_COLD;

    void flush_internal_queue(InternalQueue &);

    Vector<InternalQueue> _iqueues;

    DPDKDevice* _dev;
    int _queue_id;
    bool _blocking;
    Spinlock _lock;
    unsigned int _iqueue_size;
    unsigned int _burst_size;
    int _timeout;
    unsigned long _n_sent;
    unsigned long _n_dropped;
    bool _congestion_warning_printed;
};

CLICK_ENDDECLS

#endif
