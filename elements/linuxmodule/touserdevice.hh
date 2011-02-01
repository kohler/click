#ifndef CLICK_TOUSERDEVICE_HH
#define CLICK_TOUSERDEVICE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/notifier.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <asm/types.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/route.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

/*
=c
ToUserDevice(DEV_MINOR, [<keywords> CAPACITY, BURST])

=s netdevices
Writes packets from the click into a device's ring buffer, which can be
then read by a userlevel application.

=d
Requires --enable-experimental.

Keyword arguments are:

=over 8

=item CAPACITY

Unsigned integer.  Sets the capacity of the internal ring buffer that stores
the packets.

=item BURST

Sets the maximum number of packets returned per multipacket read.  Default is
0, which means no limit.

=back

The device must be first created in the /dev directory
via mknod /dev/fromclickX c 241 X  where X is the minor device number (i.e. 0, 1, 2)

A user level application can then read directly from the device once it opens a file
/dev/fromclickX

The user level application must know what sort of packets are written into the device. (i.e.
Eth frames, IP packets, etc.)

=n

=e

  ... -> ToUserDevice(0, CAPACITY 200);

Example how to read from the element in a userprogram when multi
mode is enabled.

   unsigned char buffer[BUFFERLEN];
   ssize_t readlen = read(fd, buffer, BUFFERLEN);
   for (ssize_t pos = 0; pos < readlen; ) {
           int len = *(int*)(buffer + pos);
           pos += sizeof(int);
	   int stored_len = (len <= readlen - pos ? len : readlen - pos);

           // At this point "buffer + pos" contains a packet that was "len"
           // bytes long.  "stored_len" of those bytes are in the buffer.
           process_packet(buffer + pos, len, stored_len);

           // shift to next packet
           pos += stored_len;
           if (pos % sizeof(int))
                   pos += sizeof(int) - pos % sizeof(int);
   }

=a
ToUserDevice */


class ToUserDevice : public Element
{
public:

    ToUserDevice();
    ~ToUserDevice();

    static void static_initialize();
    static void static_cleanup();

    const char *class_name() const      { return "ToUserDevice"; }
    const char *port_count() const      { return PORTS_1_0; }
    const char *processing() const      { return PUSH; }


    int    configure(Vector<String> &, ErrorHandler *);
    bool   process(Packet *);
    bool   run_task(Task *);
    int    initialize(ErrorHandler *);
    void   cleanup(CleanupStage);
    void   add_handlers();
    void   push(int, Packet *);
    static String read_handler(Element *e, void *thunk);

private:
    enum { default_capacity = 64 };

    Packet **	    _q;
    uint32_t	    _size;
    uint32_t	    _r_slot;	// where we read from
    uint32_t	    _w_slot;	// where we write to
    uint32_t        _capacity;
    spinlock_t      _lock;
    ulong           _read_count;
    ulong           _pkt_read_count;
    ulong	    _pkt_count;
    ulong	    _drop_count;
    ulong           _block_count;
    unsigned	    _max_burst;
    atomic_uint32_t _failed_count;
    Task            _task;
    NotifierSignal  _signal;
    volatile bool   _exit;
    // related to device management
    String                 _devname;
    ulong                  _dev_major;
    ulong                  _dev_minor;
    wait_queue_head_t      _proc_queue;
    ulong                  _sleep_proc;

    static struct file_operations *dev_fops;
#if HAVE_UNLOCKED_IOCTL
    static struct mutex _ioctl_mutex;
#endif

    static ssize_t dev_read(struct file *file, char *buf, size_t count, loff_t *ppos);
    static int     dev_open(struct inode *inode, struct file *filp);
    static int     dev_release(struct inode *inode, struct file *file);
    static uint    dev_poll(struct file *, struct poll_table_struct *);
    static int dev_ioctl(struct inode *inode, struct file *filp,
			 unsigned command, unsigned long address);
#if HAVE_UNLOCKED_IOCTL
    static long dev_unlocked_ioctl(struct file *filp, unsigned int command,
				   unsigned long address);
#endif
};

#endif
