// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMFLANDUMP_HH
#define CLICK_FROMFLANDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
class HandlerCall;

/*
=c

FromFlanDump(FILENAME [, I<KEYWORDS>])

=s traces

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

    FromFlanDump() CLICK_COLD;
    ~FromFlanDump() CLICK_COLD;

    const char *class_name() const		{ return "FromFlanDump"; }
    const char *port_count() const		{ return PORTS_0_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);
    Packet *pull(int);

    void set_active(bool);

  private:

    static const uint32_t BUFFER_SIZE = 32768;
    static const int SAMPLING_SHIFT = 28;

    class FlanFile { public:
	FlanFile();
	~FlanFile();
	int open(const String &basename, const String &filename, int record_size, ErrorHandler *);
	int read_more(off_t);
	off_t last_record() const;
	uint16_t read_uint16(off_t) const;
	uint32_t read_uint32(off_t) const;
      private:
	int _fd;
	const uint8_t *_buffer;
	off_t _offset;
	uint32_t _len;
	FILE *_pipe;
	bool _my_buffer;
	int _record_size;
	enum { BUFFER_SIZE = 65536 };
    };

    enum { FF_FLID = 0, FF_TIME, FF_SIZE, FF_FLAGS,
	   FF_SADDR, FF_DADDR, FF_DPORT, FF_CT_PKT, FF_CT_BYTES, FF_BEG,
	   FF_FIRST_PKT = FF_FLID, FF_LAST_PKT = FF_FLAGS + 1,
	   FF_FIRST_FLOW = FF_SADDR, FF_LAST_FLOW = FF_BEG + 1,
	   FF_LAST = FF_LAST_FLOW };
    FlanFile *_ff[FF_LAST];

    off_t _record;
    off_t _last_record;

    bool _flows : 1;
    bool _stop : 1;
    bool _active;

    Task _task;

    String _dirname;

    int error_helper(ErrorHandler *, const char *);
    int read_buffer(ErrorHandler *);
    int read_into(void *, uint32_t, ErrorHandler *);
    bool read_packet(ErrorHandler *);

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

inline off_t
FromFlanDump::FlanFile::last_record() const
{
    return (_offset + _len) / _record_size;
}

inline uint16_t
FromFlanDump::FlanFile::read_uint16(off_t o) const
{
    return *reinterpret_cast<const uint16_t *>(_buffer + o<<1 - _offset);
}

inline uint32_t
FromFlanDump::FlanFile::read_uint32(off_t o) const
{
    return *reinterpret_cast<const uint16_t *>(_buffer + o<<2 - _offset);
}

#endif
