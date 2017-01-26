// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMDUMP_HH
#define CLICK_FROMDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/fromfile.hh>
CLICK_DECLS
class HandlerCall;

/*
=c

FromDump(FILENAME [, I<keywords> STOP, TIMING, SAMPLE, FORCE_IP, START, START_AFTER, END, END_AFTER, INTERVAL, END_CALL, FILEPOS, MMAP])

=s traces

reads packets from a tcpdump file

=d

Reads packets from a file produced by `tcpdump -w FILENAME' or ToDump and
emits them from the output, optionally stopping the driver when there are no
more packets.

FromDump also transparently reads gzip- and bzip2-compressed tcpdump files, if
you have zcat(1) and bzcat(1) installed.

Keyword arguments are:

=over 8

=item STOP

Boolean.  If true, then FromDump will ask the router to stop when it is done
reading its tcpdump file (or the END time is reached).  Default is false.

=item TIMING

Boolean. If true, then FromDump tries to maintain the timing of the original
packet stream. The first packet is emitted immediately; thereafter, FromDump
maintains the delays between packets. Default is false.

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

=item START

Absolute time in seconds since the epoch. FromDump will output packets with
timestamps after that time.

=item START_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromDump will skip the first I<T> seconds in the log.

=item END

Absolute time in seconds since the epoch. FromDump will stop when encountering
a packet with timestamp at or after that time.

=item END_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first timestamp in the log.

=item INTERVAL

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first packet output.

=item END_CALL

Specify a handler to call once the end time is reached, or the dump runs out
of packets.  This defaults to 'I<FromDump>.active false'.  END_CALL and STOP
are mutually exclusive.

=item ACTIVE

Boolean. If false, then FromDump will not emit packets (until the `C<active>'
handler is written). Default is true.

=item FILEPOS

File offset. If supplied, then FromDump will start emitting packets from
this (uncompressed) file position. This is dangerous; there's no cheap way
to check whether you got the offset wrong, and if you did get it wrong,
FromDump will emit garbage.

=item MMAP

Boolean. If true, then FromDump will use mmap(2) to access the tcpdump file.
This can result in slightly better performance on some machines. FromDump's
regular file discipline is pretty optimized, so the difference is often small
in practice. Default is true on most operating systems, but false on Linux.

=back

You can supply at most one of START and START_AFTER, and at most one of END,
END_AFTER, and INTERVAL.

Only available in user-level processes.

=n

By default, `tcpdump -w FILENAME' dumps only the first 68 bytes of
each packet. You probably want to run `tcpdump -w FILENAME -s 2000' or some
such.

FromDump sets packets' extra length annotations to any additional length
recorded in the dump.

FromDump is a notifier signal, active when the element is active and the dump
contains more packets.

If FromDump uses mmap, then a corrupt file might cause Click to crash with a
segmentation violation.

=h count read-only

Returns the number of packets output so far.

=h reset_counts write-only

Resets "count" to 0.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns the file's encapsulation type.

=h filename read-only

Returns the filename supplied to FromDump.

=h filesize read-only

Returns the length of the FromDump file, in bytes, or "-" if that length
cannot be determined (because the file was compressed, for example).

=h filepos read/write

Returns or sets FromDump's position in the (uncompressed) file, in bytes.

=h packet_filepos read-only

Returns the (uncompressed) file position of the last packet emitted, in bytes.
This handler is useful for elements like AggregateIPFlows that can record
statistics about portions of a trace; with packet_filepos, they can note
exactly where the relevant portion begins.

=h extend_interval write-only

Text is a time interval. If END_TIME or one of its cousins was specified, then
writing to this handler extends END_TIME by that many seconds. Also, ACTIVE is
set to true.

=h reset_timing write-only

Resets timing information.  Useful when TIMING is true and you skate around in
the file by writing C<filepos>.

=a

ToDump, FromDevice.u, ToDevice.u, tcpdump(1), mmap(2), AggregateIPFlows,
FromTcpdump */

class FromDump : public Element { public:

    FromDump() CLICK_COLD;
    ~FromDump() CLICK_COLD;

    const char *class_name() const		{ return "FromDump"; }
    const char *port_count() const		{ return "0/1-2"; }
    const char *processing() const		{ return PROCESSING_A_AH; }
    void *cast(const char *);
    String declaration() const;

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    FromDump *hotswap_element() const;
    void take_state(Element *, ErrorHandler *);

    void run_timer(Timer *);
    bool run_task(Task *);
    Packet *pull(int);

    void set_active(bool);

  private:

    enum { BUFFER_SIZE = 32768, SAMPLING_SHIFT = 28 };

    FromFile _ff;

    Packet *_packet;

    bool _swapped : 1;
    bool _timing : 1;
    bool _force_ip : 1;
    bool _have_first_time : 1;
    bool _have_last_time : 1;
    bool _have_any_times : 1;
    bool _first_time_relative : 1;
    bool _last_time_relative : 1;
    bool _last_time_interval : 1;
    bool _have_nanosecond_timestamps : 1;
    bool _active;
    unsigned _extra_pkthdr_crap;
    unsigned _sampling_prob;
    int _minor_version;
    int _linktype;

    Timestamp _first_time;
    Timestamp _last_time;
    HandlerCall *_end_h;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;

    Timer _timer;
    Task _task;
    ActiveNotifier _notifier;

    Timestamp _timing_offset;
    off_t _packet_filepos;

    bool read_packet(ErrorHandler *);

    void prepare_times(const Timestamp &);
    bool check_timing(Packet *p);

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
