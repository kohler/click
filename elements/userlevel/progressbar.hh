// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_THERMOMETER_HH
#define CLICK_THERMOMETER_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
=c

FromDump(FILENAME [, TIMING, I<KEYWORDS>])

=s sources

reads packets from a tcpdump(1) file

=io

None

=d

Reads packets from a file produced by `tcpdump -w FILENAME' or ToDump. Pushes
them out the output, and optionally stops the driver when there are no more
packets. If TIMING is true, then FromDump tries to maintain the timing of the
original packet stream. TIMING is false by default.

FromDump also transparently reads gzip- and bzip2-compressed tcpdump files, if
you have zcat(1) and bzcat(1) installed.

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

=item TIMING

Boolean. Same as the TIMING argument.

=item ACTIVE

Boolean. If false, then FromDump will not emit packets (until the `C<active>'
handler is written). Default is true.

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

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns the file's encapsulation type.

=h filesize read-only

Returns the length of the FromIPSummaryDump file, in bytes, or "-" if that
length cannot be determined.

=h filepos read-only

Returns FromIPSummaryDump's position in the file, in bytes.

=a

ToDump, FromDevice.u, ToDevice.u, tcpdump(1), mmap(2) */

class Thermometer : public Element { public:

    Thermometer();
    ~Thermometer();

    const char *class_name() const		{ return "Thermometer"; }
    Thermometer *clone() const			{ return new Thermometer; }

    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();

    void run_scheduled();

  private:

#if HAVE_INT64_TYPES
    typedef uint64_t thermometer_t;
#else
    typedef uint32_t thermometer_t;
#endif

    enum { ST_FIRST, ST_MIDDLE, ST_DONE };
    
    bool _have_size;
    int _status;
    thermometer_t _size;
    thermometer_t _last_pos;
    struct timeval _start_time;
    struct timeval _last_time;

    Timer _timer;
    uint32_t _interval;
    
    Element *_size_element;
    int _size_hi;
    Element *_pos_element;
    int _pos_hi;
    
};

#endif
