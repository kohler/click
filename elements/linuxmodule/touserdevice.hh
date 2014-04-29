#ifndef CLICK_TOUSERDEVICE_HH
#define CLICK_TOUSERDEVICE_HH
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
ToUserDevice(DEV_MINOR, [I<KEYWORDS>])

=s netdevices
Pass packets to user-level programs via a character device.

=d

ToUserDevice provides a way to get packets from click running in linuxmodule
into userspace through a character device.  This is a way of giving packets to a
user process without going through linux's routing or networking code.
Internally, ToUserDevice has a queue of packets.

ToUserDevice can optionally apply encapsulation to the packets, either pcap
encapsulation or length-delimited encapsulation.

The character device can act like a datagram socket, where one packet is
returned per read()/recv() call (but the packet may end up truncated), or it can
act like a streaming socket where all packet data is reliably returned in a
stream.  In order to reliably be able to detect packet boundaries, it makes the
most sense to use one of the encapsulation types if using the streaming
interface.

In addition, ToUserDevice can run in a mode where reads will block until there
is data available (the default mode) or it can run in a mode where read() will
return 0 (end-of-file) if the device is empty.  This can be particularly useful
with the pcap format where the character device will appear to be a valid pcap
file.

=d
Requires --enable-experimental.

Keyword arguments are:

=over 8

=item CAPACITY

Unsigned integer.  Sets the capacity of the internal ring buffer that stores
the packets.  Defaults to 64.

=item BLOCKING

Boolean.  If true, then reads will block until there is data to return.  If
false, reads when the ring buffer is empty will return 0.  Defaults to true.

=item DROP_TAIL

Boolean.  Sets the drop policy of this device if the ring buffer is currently
full.  Defaults to true.

=item ENCAP

String.  Must be one of: none, pcap, len32, or len_net16.  Defaults to 'none.'
Choosing pcap will cause each new open of the character device to first receive
a valid pcap file header, and add individual pcap packet headers to each packet.
len32 prepends a uint32 in host byte order before each packet with the packet's
length.  len_net16 prepends a uint16 in network byte order before each packet
with the packet's length.

=item PCAP_NETWORK

Unsigned integer.  When using 'ENCAP pcap', this changes the reported network
type of the pcap file.  Defaults to 1 (Ethernet).

=item PCAP_SNAPLEN

Unsigned integer. This limits the amount of packet data returned by the 'pcap'
encapsulation type.  Only affects the pcap type.  Defaults to 65535.

=item TYPE

String.  Must be one of 'packet' or 'stream'.  It defaults to 'packet' if ENCAP
is 'none', and to 'stream' otherwise.

=item BURST

Unsigned integer. Sets the maximum number of packets returned per read.  Default
is 1.  Zero means unlimited.  It is likely not wise to set this to anything
except 1 if this is a 'TYPE packet' device.

=item DEBUG

Boolean.  Causes the element to print many debugging messages.  Defaults to
false.

=back

The device must be first created in the /dev directory
via 'mknod /dev/fromclickX c 241 X' where X is the minor device number (i.e. 0, 1, 2)

A user level application can then read directly from the device once it opens a file
/dev/fromclickX

=n

=a
FromUserDevice */


class ToUserDevice : public Element
{
public:

    ToUserDevice() CLICK_COLD;
    ~ToUserDevice() CLICK_COLD;

    static void static_initialize();
    static void static_cleanup();

    const char *class_name() const      { return "ToUserDevice"; }
    const char *port_count() const      { return PORTS_1_0; }
    const char *processing() const      { return PUSH; }

    int    configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int    configure_phase() const      { return CONFIGURE_PHASE_DEFAULT - 1; }
    bool   run_task(Task *);
    int    initialize(ErrorHandler *) CLICK_COLD;
    void   cleanup(CleanupStage) CLICK_COLD;
    void   add_handlers() CLICK_COLD;
    void   push(int, Packet *);

private:
    struct file_priv {
        ToUserDevice *dev;
        uint8_t read_once;
        Packet *p;
    };

    Packet *pop_packet();
    bool   process(Packet *);
    void   reset();

    enum { default_capacity = 64 };
    enum { h_reset };

    Packet **	    _q;
    uint32_t	    _size;
    uint32_t	    _r_slot;	// where we read from
    uint32_t	    _w_slot;	// where we write to
    uint32_t        _capacity;
    uint32_t        _max_burst;
    spinlock_t      _lock;
    bool	    _debug;
    bool	    _drop_tail;
    bool	    _blocking;
    ulong           _read_count;
    ulong           _pkt_read_count;
    ulong           _pkt_count;
    ulong           _drop_count;
    ulong           _block_count;
    atomic_uint32_t _failed_count;
    Task            _task;
    NotifierSignal  _signal;
    volatile bool   _exit;
    // related to device management
    ulong                  _dev_major;
    ulong                  _dev_minor;
    wait_queue_head_t      _proc_queue;
    ulong                  _sleep_proc;
    enum { type_packet, type_stream };
    uint8_t         _type;
    enum { type_encap_none, type_encap_pcap, type_encap_len32, type_encap_len_net16 };
    uint8_t         _encap_type;
    uint32_t        _pcap_network;
    uint32_t        _pcap_snaplen;
    class FromUserDevice *_from_user_device;

    static struct file_operations *dev_fops;
    static ToUserDevice * volatile elem_map[256];

    static ssize_t dev_read(struct file *file, char *buf, size_t count, loff_t *ppos);
    static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *ppos);
    static int     dev_open(struct inode *inode, struct file *filp);
    static int     dev_release(struct inode *inode, struct file *file);
    static uint    dev_poll(struct file *, struct poll_table_struct *);

    static int write_handler(const String &, Element *e, void *, ErrorHandler *) CLICK_COLD;

    friend class FromUserDevice;
};

#endif
