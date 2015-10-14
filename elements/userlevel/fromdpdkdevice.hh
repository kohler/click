#ifndef CLICK_FROMDPDKDEVICE_HH
#define CLICK_FROMDPDKDEVICE_HH

#include <click/element.hh>
#include <click/notifier.hh>
#include <click/task.hh>
#include <click/dpdkdevice.hh>

CLICK_DECLS

/*
=title FromDpdkDevice

=c

FromDpdkDevice(PORT [, QUEUE, PROMISC, BURST, NDESC])

=s netdevices

reads packets from network device using Intel's DPDK (user-level)

=d

Reads packets from the network device with DPDK port identifier PORT.

On the contrary to FromDevice.u which acts as a sniffer by default, packets
received by devices put in DPDK mode will NOT be received by the kernel, and
will thus be processed only once.

Arguments:

=over 8

=item PORT

Integer.  Port identifier of the device.

=item QUEUE

Integer.  Index of the queue to use. If omitted or negative, auto-increment
between FromDpdkDevice attached to the same port will be used.

=item PROMISC

Boolean.  FromDpdkDevice puts the device in promiscuous mode if PROMISC is
true. The default is false.

=item BURST

Integer.  Maximal number of packets that will be processed before rescheduling.
The default is 32.

=item NDESC

Integer.  Number of descriptors per ring. The default is 256.

=back

This element is only available at user level, when compiled with DPDK
support.

=e

  FromDpdkDevice(3, QUEUE 1) -> ...

=h count read-only

Returns the number of packets read by the device.

=h reset_count write-only

Resets "count" to zero.

=a DpdkInfo, ToDpdkDevice */

class FromDpdkDevice : public Element {
public:

    FromDpdkDevice() CLICK_COLD;
    ~FromDpdkDevice() CLICK_COLD;

    const char *class_name() const { return "FromDpdkDevice"; }
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

    static String count_handler(Element*, void*) CLICK_COLD;
    static int reset_count_handler(const String&, Element*, void*,
                                   ErrorHandler*) CLICK_COLD;

    unsigned int _port_id;
    int _queue_id;
    bool _promisc;
    unsigned int _burst_size;
    unsigned long _count;

    Task _task;
};

CLICK_ENDDECLS

#endif // CLICK_FROMDPDKDEVICE_HH
