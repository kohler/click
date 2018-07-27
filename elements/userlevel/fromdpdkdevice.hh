#ifndef CLICK_FROMDPDKDEVICE_HH
#define CLICK_FROMDPDKDEVICE_HH

#include <click/element.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <click/dpdkdevice.hh>

CLICK_DECLS

/*
=title FromDPDKDevice

=c

FromDPDKDevice(PORT [, QUEUE [, I<keywords> PROMISC, BURST, NDESC]])

=s netdevices

reads packets from network device using Intel's DPDK (user-level)

=d

Reads packets from the network device with DPDK port identifier PORT.

On the contrary to FromDevice.u which acts as a sniffer by default, packets
received by devices put in DPDK mode will NOT be received by the kernel, and
will thus be processed only once.

To use RSS (Receive Side Scaling) to receive packets from the same device
on multiple queues (possibly pinned to different Click threads), simply
use multiple FromDPDKDevice with the same PORT argument. Each
FromDPDKDevice will open a different RX queue attached to the same port,
and packets will be dispatched among the FromDPDKDevice elements that
you can pin to different thread using StaticThreadSched.

Arguments:

=over 8

=item PORT

Integer or PCI address.  Port identifier of the device, or a PCI address in the
format fffff:ff:ff.f

=item QUEUE

Integer.  Index of the queue to use. If omitted or negative, auto-increment
between FromDPDKDevice attached to the same port will be used.

=item PROMISC

Boolean.  FromDPDKDevice puts the device in promiscuous mode if PROMISC is
true. The default is false.

=item BURST

Integer.  Maximal number of packets that will be processed before rescheduling.
The default is 32.

=item NDESC

Integer.  Number of descriptors per ring. The default is 256.

=item ALLOW_NONEXISTENT

Boolean.  Do not fail if the PORT does not exist. If it's the case the task
will never run and this element will behave like Idle.

=item ACTIVE
Boolean.  If false, do not start reading packets from the device. Wait for a
handler call (see active handler).

=back

This element is only available at user level, when compiled with DPDK
support.

=e

  FromDPDKDevice(3, QUEUE 1) -> ...

=h count read-only

Returns the number of packets processed by this FromDPDKDevice

=h reset_count write-only

Resets "count" to zero.

=h hw_count read-only

Returns the amount of packets received by the whole device.

=h hw_bytes read-only

Returns the amount of bytes received by the whole device.

=h hw_dropped read-only

Returns the amount of packets dropped by the device.

=h hw_errors read-only

Returns the amount of packets dropped because of error by the device.

=h device read-only

Returns the DPDK port id.

=h duplex read-only

Returns the current duplex mode

=h autoneg read-only

Tells if autonegotiation is activated. Only available with DPDK 16.04+

=h speed read-only

Returns the device current speed (eg 100Gbits/s)

=h carrier read-only

Tells the current link status.

=h xstats read-only

Returns the DPDK device's extended statistics. If a parameter is given, will
return the value for the specific name only. If no parameter, will return
the current value for all statistics.

=h active

Pause/unpause the reception of packets in this FromDPDKDevice.

=h nb_rx_queues read-only

Returns the number of RX queues

=h nb_tx_queues read-only

Returns the number of TX queues

=h mac read-only

Returns the device main MAC address

=h add_mac write_only

Allows to add other MAC addresses to the device

=h remove_mac write_only

Allows to remove MAC addresses added using add_mac.

=h burst

Returns the amount of packets read in one go by this element. Contrary to
the BURST parameter, this value is not checked. Eg each device generally have
a minimal and a maximal value for BURST. A value between 4 and 256 is safe.


=a DPDKInfo, ToDPDKDevice */

class FromDPDKDevice : public Element {
public:

    FromDPDKDevice() CLICK_COLD;
    ~FromDPDKDevice() CLICK_COLD;

    const char *class_name() const { return "FromDPDKDevice"; }
    const char *port_count() const { return PORTS_0_1; }
    const char *processing() const { return PUSH; }
    int configure_phase() const {
        return CONFIGURE_PHASE_PRIVILEGED;
    }
    bool can_live_reconfigure() const { return false; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    bool run_task(Task *);

private:

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(
        const String &, Element *, void *, ErrorHandler *
    ) CLICK_COLD;
    static String status_handler(Element *e, void *thunk) CLICK_COLD;
    static String statistics_handler(Element *e, void *thunk) CLICK_COLD;
    static int xstats_handler(int operation, String &input, Element *e,
                              const Handler *handler, ErrorHandler *errh);
    enum {
        h_count, h_reset_count,
        h_driver, h_carrier, h_duplex, h_autoneg, h_speed,
        h_ipackets, h_ibytes, h_imissed, h_ierrors,
        h_active,
        h_nb_rx_queues, h_nb_tx_queues, h_nb_vf_pools,
        h_mac, h_add_mac, h_remove_mac, h_vf_mac,
        h_device,
    };

    DPDKDevice* _dev;
    int _queue_id;
    bool _promisc;
    unsigned int _burst_size;
    unsigned long _count;
    bool _active;

    Task _task;
};

CLICK_ENDDECLS

#endif // CLICK_FROMDPDKDEVICE_HH
