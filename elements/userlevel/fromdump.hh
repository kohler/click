#ifndef FROMDUMP_HH
#define FROMDUMP_HH
#include <click/element.hh>
#include <click/task.hh>

/*
=c

FromDump(FILENAME [, TIMING, I<KEYWORDS>])

=s sources

reads packets from a tcpdump(1) file

=d

Reads packets from a file produced by `tcpdump -w FILENAME' or ToDump. Pushes
them out the output, and optionally stops the driver when there are no more
packets. If TIMING is true, then FromDump tries to maintain the timing of the
original packet stream. TIMING is false by default.

Keyword arguments are:

=over 8

=item SAMPLE

Unsigned real number between 0 and 1. FromDump will output each packet with
probability SAMPLE. Default is 1. FromDump uses fixed-point arithmetic, so the
actual sampling probability may differ substantially from the requested
sampling probability. Use the C<sampling_prob> handler to find out the actual
probability.

=item FORCE_IP

Boolean. If true, then FromDump will emit only IP packets with their IP header
annotations correctly set. (If FromDump has two outputs, non-IP packets are
pushed out on output 1; otherwise, they are dropped.) Default is false.

=item STOP

Boolean. If true, then FromDump will ask the router to stop when it is done
reading its tcpdump file. Default is false.

=item ACTIVE

Boolean. If false, then FromDump will not emit packets (until the `C<active>'
handler is written). Default is true.

=item TIMING

Boolean. Same as the TIMING argument.

=item MMAP

Boolean. If true, then FromDump will use mmap(2) to access the tcpdump file.
This can result in slightly better performance on some machines. FromDump's
regular file discipline is pretty optimized, so the difference is often small
in practice. Default is true on most operating systems, but false on Linux.

=back

Only available in user-level processes.

=n

By default, `tcpdump -w FILENAME' dumps only the first 68 bytes of
each packet. You probably want to run `tcpdump -w FILENAME -s 2000' or some
such.

FromDump sets packets' extra length annotations to any additional length
recorded in the dump.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=a

ToDump, FromDevice.u, ToDevice.u, tcpdump(1), mmap(2) */

class FromDump : public Element { public:

    FromDump();
    ~FromDump();

    const char *class_name() const		{ return "FromDump"; }
    const char *processing() const		{ return "a/ah"; }
    FromDump *clone() const			{ return new FromDump; }

    void notify_noutputs(int);
    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();
    void add_handlers();

    void run_scheduled();
    Packet *pull(int);
  
  private:

    static const uint32_t BUFFER_SIZE = 32768;
    static const int SAMPLING_SHIFT = 28;
    
    int _fd;
    const unsigned char *_buffer;
    uint32_t _pos;
    uint32_t _len;
    
    WritablePacket *_data_packet;
    Packet *_packet;
    
    bool _swapped : 1;
    bool _timing : 1;
    bool _stop : 1;
    bool _force_ip : 1;
#ifdef ALLOW_MMAP
    bool _mmap : 1;
#endif
    bool _active;
    unsigned _extra_pkthdr_crap;
    unsigned _sampling_prob;
    int _minor_version;
    int _linktype;

#ifdef ALLOW_MMAP
    static const uint32_t WANT_MMAP_UNIT = 4194304; // 4 MB
    size_t _mmap_unit;
    off_t _mmap_off;
#endif
    
    Task _task;
  
    struct timeval _time_offset;
    String _filename;

    int error_helper(ErrorHandler *, const char *);
#ifdef ALLOW_MMAP
    int read_buffer_mmap(ErrorHandler *);
#endif
    int read_buffer(ErrorHandler *);
    int read_into(void *, uint32_t, ErrorHandler *);
    bool check_force_ip(Packet *);
    Packet *read_packet(ErrorHandler *);

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);
    
};

#endif
