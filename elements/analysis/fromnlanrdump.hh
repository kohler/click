// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMNLANRDUMP_HH
#define CLICK_FROMNLANRDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <clicknet/tcp.h>
#include <click/fromfile.hh>
CLICK_DECLS
class HandlerCall;

/*
=c

FromNLANRDump(FILENAME [, I<KEYWORDS>])

=s traces

reads packets from an NLANR file

=d

Reads IP packets from a file in a format used for NLANR traces: either FR,
FR+, or TSH. Pushes them out the output, and optionally stops the driver when
there are no more packets.

FromNLANRDump also transparently reads gzip- and bzip2-compressed files, if
you have zcat(1) and bzcat(1) installed.

Keyword arguments are:

=over 8

=item FORMAT

String.  Should be either 'fr', 'fr+', 'tsh', or 'guess'.  Default is 'guess'.

=item STOP

Boolean.  If true, then FromNLANRDump will ask the router to stop when it is
done reading its file (or the END time is reached).  Default is false.

=item ACTIVE

Boolean. If false, then FromNLANRDump will not emit packets (until the
`C<active>' handler is written). Default is true.

=item FORCE_IP

Boolean. This argument is ignored; it's here for compatibility with FromDump
and the like. FromNLANRDump behaves as if FORCE_IP was set to true.

=item START

Absolute time in seconds since the epoch. FromNLANRDump will output packets with
timestamps after that time.

=item START_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromNLANRDump will skip the first I<T> seconds in the log.

=item END

Absolute time in seconds since the epoch. FromNLANRDump will stop when
encountering a packet with timestamp at or after that time.

=item END_AFTER

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromNLANRDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first timestamp in the log.

=item INTERVAL

Argument is relative time in seconds (or supply a suffix like `min', `h').
FromNLANRDump will stop at the first packet whose timestamp is at least I<T>
seconds after the first packet output.

=item END_CALL

Specify a handler to call once the end time is reached, or the dump runs out
of packets.  This defaults to 'I<FromNLANRDump>.active false'.  END_CALL and
STOP are mutually exclusive.

=item SAMPLE

Unsigned real number between 0 and 1. FromNLANRDump will output each packet with
probability SAMPLE. Default is 1. FromNLANRDump uses fixed-point arithmetic, so
the actual sampling probability may differ substantially from the requested
sampling probability. Use the C<sampling_prob> handler to find out the actual
probability.

=item TIMING

Boolean. If true, then FromNLANRDump tries to maintain the inter-packet timing
of the original packet stream. False by default.

=item MMAP

Boolean. If true, then FromNLANRDump will use mmap(2) to access the tcpdump
file. This can result in slightly better performance on some machines.
FromNLANRDump's regular file discipline is pretty optimized, so the difference
is often small in practice. Default is true on most operating systems, but
false on Linux.

=item FILEPOS

File offset. If supplied, then FromNLANRDump will start emitting packets from
this (uncompressed) file position. This is dangerous; if you get the offset
wrong, FromNLANRDump will emit garbage.

=back

You can supply at most one of START and START_AFTER, and at most one of END,
END_AFTER, and INTERVAL.

Only available in user-level processes.

=n

In TSH dumps, FromNLANRDump sets packets' link annotations to the link number
stored in the dump.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns "IP".

=h filename read-only

Returns the filename supplied to FromNLANRDump.

=h filesize read-only

Returns the length of the FromNLANRDump file, in bytes, or "-" if that length
cannot be determined (because the file was compressed, for example).

=h filepos read-only

Returns FromNLANRDump's position in the (uncompressed) file, in bytes.

=h packet_filepos read-only

Returns the (uncompressed) file position of the last packet emitted, in bytes.
This handler is useful for elements like AggregateIPFlows that can record
statistics about portions of a trace; with packet_filepos, they can note
exactly where the relevant portion begins.

=h extend_interval write-only

Text is a time interval. If END_TIME or one of its cousins was specified, then
writing to this handler extends END_TIME by that many seconds. Also, ACTIVE is
set to true.

=a

FromDump, ToDump, mmap(2) */

class FromNLANRDump : public Element { public:

    FromNLANRDump() CLICK_COLD;
    ~FromNLANRDump() CLICK_COLD;

    const char *class_name() const		{ return "FromNLANRDump"; }
    const char *port_count() const		{ return "0/1-2"; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);
    Packet *pull(int);

    void set_active(bool);

  private:

    enum { C_FR, C_FRPLUS, C_TSH };

    struct FRCell {
	uint32_t timestamp_sec;
	uint32_t timestamp_usec;
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t ip_len;
	uint8_t ip_p;
	uint8_t th_flags;
	uint16_t sport;
	uint16_t dport;
	enum { SIZE = 24 };
    };

    struct FRPlusCell {
	uint32_t timestamp_sec;
	uint32_t timestamp_usec;
	click_ip iph;
	click_tcp tcph;		// last 4 bytes left off
	enum { SIZE = 44 };
    };

    struct TSHCell {
	uint32_t timestamp_sec;
	uint32_t timestamp_usec; // upper 8 bits are interface #
	click_ip iph;
	click_tcp tcph;		// last 4 bytes left off
	enum { SIZE = 44 };
    };

    static const uint32_t BUFFER_SIZE = 32768;
    static const int SAMPLING_SHIFT = 28;

    FromFile _ff;

    Packet *_packet;

    bool _timing : 1;
    bool _have_first_time : 1;
    bool _have_last_time : 1;
    bool _have_any_times : 1;
    bool _first_time_relative : 1;
    bool _last_time_relative : 1;
    bool _last_time_interval : 1;
    bool _active;
    unsigned _sampling_prob;
    int _format;
    int _cell_size;

    Timestamp _first_time;
    Timestamp _last_time;
    HandlerCall *_end_h;

    Task _task;

    Timestamp _time_offset;
    off_t _packet_filepos;

    bool read_packet(ErrorHandler *);

    void prepare_times(const Timestamp &);

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
