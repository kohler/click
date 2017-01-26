// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMDAGDUMP_HH
#define CLICK_FROMDAGDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/fromfile.hh>
CLICK_DECLS
class HandlerCall;

/*
=c

FromDAGDump(FILENAME [, I<KEYWORDS>])

=s traces

reads packets from a DAG/ERF file

=d

Reads packets from a file in ERF format, produced by the University of
Waikato's DAG tools. Pushes them out the output, and optionally stops the
driver when there are no more packets.

FromDAGDump also transparently reads gzip- and bzip2-compressed files, if you
have zcat(1) and bzcat(1) installed.

Keyword arguments are:

=over 8

=item STOP

Boolean.  If true, then FromDAGDump will ask the router to stop when it is
done reading its file (or the END time is reached).  Default is false.

=item ACTIVE

Boolean. If false, then FromDAGDump will not emit packets (until the
`C<active>' handler is written). Default is true.

=item FORCE_IP

Boolean. If true, then FromDAGDump will emit only IP packets with their IP
header annotations correctly set. (If FromDAGDump has two outputs, non-IP
packets are pushed out on output 1; otherwise, they are dropped.) Default is
false.

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

Specify a handler to call once the end time is reached, or the dump runs out
of packets.  This defaults to 'I<FromDAGDump>.active false'.  END_CALL and
STOP are mutually exclusive.

=item SAMPLE

Unsigned real number between 0 and 1. FromDAGDump will output each packet with
probability SAMPLE. Default is 1. FromDAGDump uses fixed-point arithmetic, so
the actual sampling probability may differ substantially from the requested
sampling probability. Use the C<sampling_prob> handler to find out the actual
probability.

=item TIMING

Boolean. If true, then FromDAGDump tries to maintain the inter-packet timing
of the original packet stream. False by default.

=item ENCAP

Legacy encapsulation type ("IP", "ATM", "SUNATM", "ETHER", "PPP", or
"PPP_HDLC").  New-style ERF dumps contain an explicit encapsulation type on
each packet; you should not provide an ENCAP option for new-style ERF dumps.
Legacy-format dumps don't contain any encapsulation information, however, so
you should supply an encapsulation type explicitly (or FromDAGDump will assume
ENCAP type "ATM").

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

FromDAGDump sets packets' extra length annotations to any additional length
recorded in the dump.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns the previous packet's encapsulation type.

=h filename read-only

Returns the filename supplied to FromDAGDump.

=h filesize read-only

Returns the length of the FromDAGDump file, in bytes, or "-" if that length
cannot be determined (because the file was compressed, for example).

=h filepos read-only

Returns FromDAGDump's position in the (uncompressed) file, in bytes.

=h extend_interval write-only

Text is a time interval. If END_TIME or one of its cousins was specified, then
writing to this handler extends END_TIME by that many seconds. Also, ACTIVE is
set to true.

=a

FromDump, ToDump, mmap(2) */

class FromDAGDump : public Element { public:

    FromDAGDump() CLICK_COLD;
    ~FromDAGDump() CLICK_COLD;

    const char *class_name() const		{ return "FromDAGDump"; }
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

    struct DAGPosCell {
	uint32_t hdlc;
	uint8_t payload[44];
    };
    struct DAGEthernetCell {
	uint8_t offset;
	uint8_t padding;
	uint8_t ether_dhost[6];
	uint8_t ether_shost[6];
	uint16_t ether_type;
	uint8_t payload[32];
    };
    struct DAGATMCell {
	uint32_t header;
	uint8_t payload[44];
    };
    struct DAGAAL5Cell {
	uint32_t header;
	uint8_t payload[44];
    };
    struct DAGCell {
	uint64_t timestamp;
	uint8_t type;
	uint8_t flags;
	uint16_t rlen;
	uint16_t lctr;
	uint16_t wlen;
	union {
	    DAGPosCell pos;
	    DAGEthernetCell ether;
	    DAGATMCell atm;
	    DAGAAL5Cell aal5;
	} u;
	enum { HEADER_SIZE = 16, CELL_SIZE = 64 };
	enum { TYPE_LEGACY = 0, TYPE_HDLC_POS = 1, TYPE_ETH = 2, TYPE_ATM = 3,
	       TYPE_AAL5 = 4, TYPE_MAX = TYPE_AAL5 };
    };

    static const uint32_t BUFFER_SIZE = 32768;
    static const int SAMPLING_SHIFT = 28;
    static const int dagtype2linktype[];

    FromFile _ff;

    Packet *_packet;

    bool _timing : 1;
    bool _force_ip : 1;
    bool _have_first_time : 1;
    bool _have_last_time : 1;
    bool _have_any_times : 1;
    bool _first_time_relative : 1;
    bool _last_time_relative : 1;
    bool _last_time_interval : 1;
    bool _active;
    unsigned _sampling_prob;
    int _linktype;
    int _base_linktype;

    Timestamp _first_time;
    Timestamp _last_time;
    HandlerCall *_end_h;

    Task _task;

    Timestamp _time_offset;

    bool read_packet(ErrorHandler *);

    void stamp_to_time(uint64_t, Timestamp &) const;
    void prepare_times(const Timestamp &);

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
