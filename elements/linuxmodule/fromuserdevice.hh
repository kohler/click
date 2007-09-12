#ifndef CLICK_FROMUSERDEVICE_HH
#define CLICK_FROMUSERDEVICE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <linux/fs.h>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/netdevice.h>
#include <linux/route.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#define SLOT_SIZE (1536+16)
#define CAPACITY  64

/*
=c
FromUserDevice(DEV_MINOR, [<keywords> CAPACITY])
 
=s netdevices
Reads packets from the device's ring buffer and injects them into Click
 
=d
Requires --enable-experimental.

Keyword arguments are:

=over 8

=item CAPACITY
Unsigned integer.  Sets the CAPACITY of the internal ring buffer that stores the packets.

=back

The device must be first created in the /dev directory
via mknod /dev/toclickX c 240 X  where X is the minor device number (i.e. 0, 1, 2)

A user level application can then write directly into the device once it opens a file
/dev/toclickX
The FromUserDevice element expects that complete IP packets are written into the device.

=n


=a
ToUser */


class FromUserDevice : public Element
{
 public:

    FromUserDevice();
    ~FromUserDevice();

    static void static_initialize();
    static void static_cleanup();
    
    const char *class_name() const      { return "FromUserDevice"; }
    const char *port_count() const      { return PORTS_0_1; }
    const char *processing() const      { return PULL; }

    int  configure(Vector<String> &, ErrorHandler *);
    int  initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();
    
    static String read_handler(Element *e, void *thunk);

    Packet* pull(int port);

    static ssize_t dev_write (struct file *file, const char *buf, size_t count, loff_t *ppos);
    static int     dev_open (struct inode *inode, struct file *filp);
    static int     dev_release (struct inode *inode, struct file *file);
    static uint    dev_poll(struct file *, struct poll_table_struct *);

private:
    struct slot
    {
        uint32_t size;
        u_char   buff[SLOT_SIZE];
    };

    String          _devname;
    ulong           _size;
    struct slot    *_buff;
    ulong           _slot_size;
    ulong           _r_slot; // where we read from
    ulong           _w_slot; // where we write to
    ulong           _capacity;
    spinlock_t      _lock;
    ulong           _write_count;
    ulong           _drop_count;
    ulong           _pkt_count;
    ulong           _block_count;
    atomic_uint32_t _failed_count;
    bool            _exit;

    // related to the device management
    ulong                  _sleep_proc;
    wait_queue_head_t      _proc_queue;
    ulong                  _dev_major;
    ulong                  _dev_minor;
    static struct file_operations *dev_fops;
};

#endif
