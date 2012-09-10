// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TOIPFLOWDUMPS_HH
#define CLICK_TOIPFLOWDUMPS_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <click/straccum.hh>
#include <click/task.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <clicknet/tcp.h>
#include "aggregatenotifier.hh"
CLICK_DECLS

/*
=c

ToIPFlowDumps(FILEPATTERN [, I<KEYWORDS>])

=s traces

creates separate trace files for each TCP/UDP flow

=d

Writes summary information about incoming packets, in the style of
ToIPSummaryDump, to several files, one file per flow. It distinguishes flows
by their aggregate annotations. You usually will run ToIPFlowDumps downstream
of an AggregateIPFlows element.

The FILEPATTERN argument gives the pattern used by ToIPSummaryDump to
generate filenames. Printf-like `C<%>' escapes in the pattern are expanded
differently for each flow. Available escapes are:

    %n      Aggregate annotation in decimal.
    %.0n    Upper 8 bits of aggregate annotation in decimal.
    %.1n, %.2n, %.3n   Similar for bits 16-23, 8-15, 0-7.
    %.4n    Upper 16 bits of aggregate annotation in decimal.
    %.5n    Lower 16 bits of aggregate annotation in decimal.
    %x, %X  Aggregate annotation in hex.
    %.0x, %.1x, ..., %.5x, %.0X, %.1X, ..., %.5X
            Like %.0n, ..., %.5n in hex.
    %s      Source IP address.
    %.0s, %.1s, %.2s, %.3s
            First through fourth bytes of source IP address.
    %d      Destination IP address.
    %.0d, %.1d, %.2d, %.3d
            First through fourth bytes of destination IP address.
    %S      Source port.
    %D	    Destination port.
    %p      Protocol ('T' for TCP, 'U' for UDP).
    %%      A single % sign.

You may also use the `C<0>' flag and an optional field width, so `C<%06n>'
expands to the aggregate annotation, padded on the left with enough zeroes to
make at least 6 digits.

Keyword arguments are:

=over 8

=item NOTIFIER

The name of an AggregateNotifier element, like AggregateIPFlows. If given,
then ToIPFlowDumps will ask the element for notification when flows are
deleted. It uses that notification to free its state early. It's a very good
idea to supply a NOTIFIER.

=item ABSOLUTE_TIME

Boolean. If true, print absolute timestamps instead of relative timestamps.
Defaults to false.

=item ABSOLUTE_SEQ

Boolean. If true, print absolute sequence numbers instead of relative
ones. Defaults to false.

=item BINARY

Boolean. If true, then output binary records instead of ASCII lines. Defaults
to false.

=item GZIP

Boolean. If true, then run C<gzip> to compress completed trace files. (The
resulting files have F<.gz> appended to their FILEPATTERN names.) Defaults
to false.

=item ALL_TCP_OPT

Boolean. If true, then output any non-padding TCP options present on TCP
packets. Defaults to false.

=item TCP_OPT

Boolean. If true, then output any MSS, window scaling, and SACK options
present on TCP packets. Defaults to false.

=item TCP_WINDOW

Boolean. If true, then output each TCP packet's window field. Defaults to
false.

=item IP_ID

Boolean. If true, then output packets' IP IDs. Defaults to false.

=item MINCOUNT

Unsigned. Generate output only for flows with at least MINCOUNT packets.
Defaults to 0 (output all flows).

=back

=n

Only available in user-level processes.

=e

This element

  ... -> ToIPFlowDumps(/tmp/flow%03n);

might create a file C</tmp/flow001> with the following contents.

  !IPSummaryDump 1.1
  !data timestamp direction tcp_flags tcp_seq payload_len tcp_ack
  !flowid 192.150.187.37 3153 18.26.4.44 21 T
  !first_seq > 2195313811
  !first_seq < 2484225252
  !first_time 1018330170.887165
  0.000001 > S 0 0 0
  0.075539 < SA 0 0 1

Note that sequence numbers have been offset, so that the first sequence
numbers seen by ToIPFlowDumps are output as 0. The `C<!first_seq>' comments
let you reconstruct actual sequence numbers if necessary. Similarly, timestamp
annotations are relative to `C<!first_time>'.

=a

FromIPSummaryDump, ToIPSummaryDump, AggregateIPFlows */

class ToIPFlowDumps : public Element, public AggregateListener { public:

    ToIPFlowDumps() CLICK_COLD;
    ~ToIPFlowDumps() CLICK_COLD;

    const char *class_name() const	{ return "ToIPFlowDumps"; }
    const char *port_count() const	{ return "1/0-1"; }

    enum { CONFIGURE_PHASE = CONFIGURE_PHASE_DEFAULT };
    int configure_phase() const		{ return CONFIGURE_PHASE; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void push(int, Packet *);
    Packet *pull(int);
    bool run_task(Task *);

    void aggregate_notify(uint32_t, AggregateEvent, const Packet *);

    bool absolute_time() const		{ return _absolute_time; }
    bool absolute_seq() const		{ return _absolute_seq; }
    String output_pattern() const;
    virtual void add_note(uint32_t, const String &, ErrorHandler * = 0);

    struct Pkt {
	Timestamp timestamp;
	tcp_seq_t th_seq;
	tcp_seq_t th_ack;
	uint8_t direction;
	uint8_t th_flags;
	uint16_t payload_len;
    };
    struct Note {
	int before_pkt;
	uint32_t pos;
    };

  private:

    class Flow { public:

	Flow(const Packet *, const String &, bool absolute_time, bool absolute_seq, bool binary, bool ip_id, int tcp_opt, bool tcp_window);
	~Flow();

	uint32_t aggregate() const	{ return _aggregate; }
	uint32_t npackets() const	{ return _packet_count; }
	String filename() const		{ return _filename; }
	Flow *next() const		{ return _next; }
	void set_next(Flow *f)		{ _next = f; }

	int add_pkt(const Packet *, ErrorHandler *);
	int add_note(const String &, ErrorHandler *);

	int output(ErrorHandler *);
	void compress(ErrorHandler *);
	inline void unlink(ErrorHandler *);

      private:

	enum { NPKT = 128, NNOTE = 32 };

	Flow *_next;
	IPFlowID _flowid;
	int _ip_p;
	uint32_t _aggregate;
	uint32_t _packet_count;
	uint32_t _note_count;
	String _filename;
	bool _outputted : 1;
	bool _binary : 1;
	int _tcp_opt;
	int _npkt;
	int _nnote;
	Timestamp _first_timestamp;
	bool _have_first_seq[2];
	tcp_seq_t _first_seq[2];
	Pkt _pkt[NPKT];
	Note _note[NNOTE];
	StringAccum _note_text;
	StringAccum _opt_info;
	uint16_t *_ip_ids;
	uint16_t *_tcp_windows;

	int create_directories(const String &, ErrorHandler *);
	void output_binary(StringAccum &);
	void store_opt(const click_tcp *, int direction);

    };

    enum { FLOWMAP_BITS = 10, NFLOWMAP = 1 << FLOWMAP_BITS };
    Flow *_flowmap[NFLOWMAP];
    int _nflows;

    String _filename_pattern;
    String _output_banner;

    uint32_t _nnoagg;
    uint32_t _nagg;
    AggregateNotifier *_agg_notifier;

    bool _absolute_time : 1;
    bool _absolute_seq : 1;
    bool _binary : 1;
    bool _gzip : 1;
    bool _ip_id : 1;
    bool _tcp_window : 1;
    int _tcp_opt;

    uint32_t _mincount;

    Task _task;
    NotifierSignal _signal;

    Timer _gc_timer;
    Vector<uint32_t> _gc_aggs;

    Vector<String> _compressables;
    int _compress_child;

    String expand_filename(const Packet *, ErrorHandler *) const;
    Flow *find_aggregate(uint32_t, const Packet * = 0);
    void end_flow(Flow *, ErrorHandler *);
    int add_compressable(const String &, ErrorHandler *);
    inline void smaction(Packet *);
    static void gc_hook(Timer *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler*) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
