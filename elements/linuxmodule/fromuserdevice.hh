#ifndef CLICK_FROMUSERDEVICE_HH
#define CLICK_FROMUSERDEVICE_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include <click/atomic.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/wait.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

/*
=c
FromUserDevice(TO_USER_DEVICE, [<keywords>])

=s netdevices
Emit packets written to a character device.

=d

Keyword arguments are:

=over 8

=item TO_USER_DEVICE

ToUserDevice element.  This is the ToUserDevice element that set up the actual
character device.  The element must be of 'TYPE packet,' as FromUserDevice does
not support the streaming interfaces or encapsulations.  If only the
FromUserDevice functionality is desired, then just include
"Idle -> my_tud :: ToUserDevice(...);" in the config.

=item CAPACITY

Unsigned integer.  Sets the CAPACITY of the internal ring buffer that stores the
packets.  Defaults to 64.

=item HEADROOM

Unsigned integer.  Sets the headroom on packets.  Defaults to the default
headroom.

=item TAILROOM

Unsigned integer.  Sets the tailroom on packets.  Defaults to 0.

=item MAX_PACKET_SIZE

Unsigned integer.  Sets the maximum packet size in bytes that this element will
accept.  Calls to write/send with a larger size will return with errno EMSGSIZE.
Defaults to 1536.

=back

FromUserDevice makes no assumptions about the data being written into it.  It is
necessary to use MarkMACHeader/CheckIPHeader elements if header annotations
should be set.

=n

=a
ToUserDevice */


class FromUserDevice : public Element
{
 public:

    FromUserDevice() CLICK_COLD;
    ~FromUserDevice() CLICK_COLD;

    const char *class_name() const      { return "FromUserDevice"; }
    const char *port_count() const      { return PORTS_0_1; }
    const char *processing() const      { return PULL; }

    void *cast(const char *);
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet* pull(int port);

    ssize_t dev_write(const char *buf, size_t count, loff_t *ppos);
    uint dev_poll();

private:
    ActiveNotifier _empty_note;

    // ring buffer state
    spinlock_t _lock;
    uint32_t _size;
    Packet **_buff;
    uint32_t _r_slot; // where we read from
    uint32_t _w_slot; // where we write to

    // configuration
    uint32_t _capacity;
    uint32_t _max_pkt_size;
    uint32_t _headroom;
    uint32_t _tailroom;

    // stats
    uint32_t _write_count;
    uint32_t _drop_count;
    uint32_t _pkt_count;
    uint32_t _block_count;
    uint32_t _max;
    uint32_t _failed_count;
    bool _exit;

    // related to the device management
    uint32_t _sleep_proc;
    wait_queue_head_t _proc_queue;
};

#endif
