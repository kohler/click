// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMDAGDUMP_HH
#define CLICK_FROMDAGDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
class HandlerCall;

/*
=c

FromDAGDump(FILENAME [, TIMING, I<KEYWORDS>])

=s analysis

reads packets from a DAG file

=d

Reads packets from a file in DAG format, produced by the University of
Waikato's DAG tools. Pushes them out the output, and optionally stops the
driver when there are no more packets. If TIMING is true, then FromDAGDump
tries to maintain the timing of the original packet stream. TIMING is false by
default.

FromDAGDump also transparently reads gzip- and bzip2-compressed files, if you
have zcat(1) and bzcat(1) installed.

Keyword arguments are:

=over 8

=item SAMPLE

Unsigned real number between 0 and 1. FromDAGDump will output each packet with
probability SAMPLE. Default is 1. FromDAGDump uses fixed-point arithmetic, so
the actual sampling probability may differ substantially from the requested
sampling probability. Use the C<sampling_prob> handler to find out the actual
probability.

=item FORCE_IP

Boolean. If true, then FromDAGDump will emit only IP packets with their IP
header annotations correctly set. (If FromDAGDump has two outputs, non-IP
packets are pushed out on output 1; otherwise, they are dropped.) Default is
false.

=item STOP

Boolean. If true, then FromDAGDump will ask the router to stop when it is done
reading its tcpdump file. Default is false.

=item START

Absolute time in seconds since the epoch. FromDAGDump will output packets with
timestamps after that time.

=item START_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromDAGDump will skip the first I<T> seconds in the log.

=item END

Absolute time in seconds since the epoch. FromDAGDump will stop when
encountering a packet with timestamp at or after that time.

=item END_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromDAGDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first timestamp in the log.

=item INTERVAL

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromDAGDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first packet output.

=item END_CALL

Specify the handler to call, instead of stopping FromDAGDump, once the end
time is reached.

=item TIMING

Boolean. Same as the TIMING argument.

=item ACTIVE

Boolean. If false, then FromDAGDump will not emit packets (until the
`C<active>' handler is written). Default is true.

=item MMAP

Boolean. If true, then FromDAGDump will use mmap(2) to access the tcpdump
file. This can result in slightly better performance on some machines.
FromDAGDump's regular file discipline is pretty optimized, so the difference
is often small in practice. Default is true on most operating systems, but
false on Linux.

=back

You can supply at most one of START and START_AFTER, and at most one of END,
END_AFTER, and INTERVAL.

Only available in user-level processes.

=n

By default, `tcpdump -w FILENAME' dumps only the first 68 bytes of
each packet. You probably want to run `tcpdump -w FILENAME -s 2000' or some
such.

FromDAGDump sets packets' extra length annotations to any additional length
recorded in the dump.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns the file's encapsulation type.

=h filesize read-only

Returns the length of the FromDAGDump file, in bytes, or "-" if that
length cannot be determined.

=h filepos read-only

Returns FromDAGDump's position in the file, in bytes.

=h extend_interval write-only

Text is a time interval. If END_TIME or one of its cousins was specified, then
writing to this handler extends END_TIME by that many seconds. Also, ACTIVE is
set to true.

=a

FromDump, ToDump, mmap(2) */

class FromDAGDump : public Element { public:

    FromDAGDump();
    ~FromDAGDump();

    const char *class_name() const		{ return "FromDAGDump"; }
    const char *processing() const		{ return "a/ah"; }
    FromDAGDump *clone() const			{ return new FromDAGDump; }

    void notify_noutputs(int);
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void run_scheduled();
    Packet *pull(int);

    void set_active(bool);
    
  private:

    struct DAGCell {
	uint64_t timestamp;
	uint32_t crc;
	uint32_t atm;
	uint8_t payload[48];
	static const int PAYLOAD_OFFSET = 16;
    };
    
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
    bool _have_first_time : 1;
    bool _have_last_time : 1;
    bool _have_any_times : 1;
    bool _first_time_relative : 1;
    bool _last_time_relative : 1;
    bool _last_time_interval : 1;
    bool _active;
    unsigned _sampling_prob;
    int _linktype;

#ifdef ALLOW_MMAP
    static const uint32_t WANT_MMAP_UNIT = 4194304; // 4 MB
    size_t _mmap_unit;
    off_t _mmap_off;
#endif

    struct timeval _first_time;
    struct timeval _last_time;
    HandlerCall *_last_time_h;
    
    Task _task;

    struct timeval _time_offset;
    String _filename;
    FILE *_pipe;
    off_t _file_offset;

    int error_helper(ErrorHandler *, const char *);
#ifdef ALLOW_MMAP
    int read_buffer_mmap(ErrorHandler *);
#endif
    int read_buffer(ErrorHandler *);
    int read_into(void *, uint32_t, ErrorHandler *);
    bool read_packet(ErrorHandler *);

    void stamp_to_timeval(uint64_t, struct timeval &) const;
    void prepare_times(struct timeval &);

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);
    
};

#endif
