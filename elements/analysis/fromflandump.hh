// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMFLANDUMP_HH
#define CLICK_FROMFLANDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
class HandlerCall;

/*
=c

FromFlanDump(FILENAME [, TIMING, I<KEYWORDS>])

=s analysis

reads packets from a DAG file

=d

Reads packets from a file in DAG format, produced by the University of
Waikato's DAG tools. Pushes them out the output, and optionally stops the
driver when there are no more packets. If TIMING is true, then FromFlanDump
tries to maintain the timing of the original packet stream. TIMING is false by
default.

FromFlanDump also transparently reads gzip- and bzip2-compressed files, if you
have zcat(1) and bzcat(1) installed.

Keyword arguments are:

=over 8

=item SAMPLE

Unsigned real number between 0 and 1. FromFlanDump will output each packet with
probability SAMPLE. Default is 1. FromFlanDump uses fixed-point arithmetic, so
the actual sampling probability may differ substantially from the requested
sampling probability. Use the C<sampling_prob> handler to find out the actual
probability.

=item STOP

Boolean. If true, then FromFlanDump will ask the router to stop when it is done
reading its tcpdump file. Default is false.

=item START

Absolute time in seconds since the epoch. FromFlanDump will output packets with
timestamps after that time.

=item START_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromFlanDump will skip the first I<T> seconds in the log.

=item END

Absolute time in seconds since the epoch. FromFlanDump will stop when
encountering a packet with timestamp at or after that time.

=item END_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromFlanDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first timestamp in the log.

=item INTERVAL

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromFlanDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first packet output.

=item END_CALL

Specify the handler to call, instead of stopping FromFlanDump, once the end
time is reached.

=item TIMING

Boolean. Same as the TIMING argument.

=item ACTIVE

Boolean. If false, then FromFlanDump will not emit packets (until the
`C<active>' handler is written). Default is true.

=item MMAP

Boolean. If true, then FromFlanDump will use mmap(2) to access the tcpdump
file. This can result in slightly better performance on some machines.
FromFlanDump's regular file discipline is pretty optimized, so the difference
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

FromFlanDump sets packets' extra length annotations to any additional length
recorded in the dump.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns the file's encapsulation type.

=h filesize read-only

Returns the length of the FromFlanDump file, in bytes, or "-" if that
length cannot be determined.

=h filepos read-only

Returns FromFlanDump's position in the file, in bytes.

=h extend_interval write-only

Text is a time interval. If END_TIME or one of its cousins was specified, then
writing to this handler extends END_TIME by that many seconds. Also, ACTIVE is
set to true.

=a

FromDump, ToDump, mmap(2) */

class FromFlanDump : public Element { public:

    FromFlanDump();
    ~FromFlanDump();

    const char *class_name() const		{ return "FromFlanDump"; }
    const char *processing() const		{ return AGNOSTIC; }
    FromFlanDump *clone() const			{ return new FromFlanDump; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void run_scheduled();
    Packet *pull(int);

    void set_active(bool);
    
  private:

    static const uint32_t BUFFER_SIZE = 32768;
    static const int SAMPLING_SHIFT = 28;
    
    struct FlanFile {
	int fd;
	const uint8_t *buffer;
	uint32_t pos;
	uint32_t len;
	FlanFile()		: fd(-1), buffer(0) { }
	~FlanFile();
    };

    FlanFile *_flid;
    FlanFile *_time;
    FlanFile *_size;
    FlanFile *_flags;
    
    FlanFile *_saddr;
    FlanFile *_sport;
    FlanFile *_daddr;
    FlanFile *_dport;
    FlanFile *_ct_pkt;
    FlanFile *_ct_bytes;

    bool _flows : 1;
    bool _swapped : 1;
    bool _timing : 1;
    bool _stop : 1;
    bool _force_ip : 1;
    bool _active;
    unsigned _sampling_prob;

    Task _task;

    struct timeval _time_offset;
    String _filename;
    off_t _file_offset;

    int error_helper(ErrorHandler *, const char *);
    int read_buffer(ErrorHandler *);
    int read_into(void *, uint32_t, ErrorHandler *);
    bool read_packet(ErrorHandler *);

    void stamp_to_timeval(uint64_t, struct timeval &) const;
    void prepare_times(struct timeval &);

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);
    
};

#endif
