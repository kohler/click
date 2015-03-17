// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TODUMP_HH
#define CLICK_TODUMP_HH
#include <click/timer.hh>
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <stdio.h>
CLICK_DECLS

/*
=c

ToDump(FILENAME [, I<keywords> SNAPLEN, ENCAP, USE_ENCAP_FROM, EXTRA_LENGTH])

=s traces

writes packets to a tcpdump file

=d

Writes incoming packets to FILENAME in `tcpdump -w' format. This file can be
read by `tcpdump -r', or by FromDump on a later run. FILENAME can be `-', in
which case ToDump writes to the standard output.

Writes at most SNAPLEN bytes of each packet to the file. The default SNAPLEN
is 2000. If SNAPLEN is 0, the whole packet will be written to the file.  ENCAP
specifies the first header each packet is expected to have.  This information
is stored in the file header, and must be correct or tcpdump won't be able to
read the file correctly. It can be C<ETHER> (Ethernet encapsulation),
C<IP> (raw IP packets), C<FDDI>, C<ATM>, C<802_11>, C<SLL>, C<AIRONET>, C<HDLC>,
C<PPP_HDLC>, C<PPP>, C<SUNATM>, C<PRISM>, or C<NULL>; the default is C<ETHER>.

ToDump may have zero or one output. If it has an output, then it emits all
received packets on that output. ToDump will schedule itself on the task list
if it is used as a pull element with no outputs.

Keyword arguments are:

=over 8

=item SNAPLEN

Integer.  See above.

=item ENCAP

The encapsulation type to store in the dump.  See above.

=item USE_ENCAP_FROM

Argument is a space-separated list of element names. At initialization time,
ToDump will check these elements' `encap' handlers, and parse them as ENCAP
arguments. If all the handlers agree, ToDump will use that encapsulation type;
otherwise, it will report an error. You can specify at most one of ENCAP and
USE_ENCAP_FROM. FromDump and FromDevice.u have `encap' handlers.

=item EXTRA_LENGTH

Boolean. Set to true if you want ToDump to store any extra length as recorded
in packets' extra length annotations. Default is true.

=item UNBUFFERED

Boolean. Set to true if you want ToDump to use unbuffered IO when saving data to
a file.  This is unlikely to work with compressed dump formats. Default is
false.

=item NANO

Boolean. Set to true to write nanosecond-precision timestamps. Default
is true if Click was compiled with nanosecond-precision timestamps, false
otherwise.

=back

This element is only available at user level.

=n

ToDump stores packets' true length annotations when available.

=h count read-only

Returns the number of packets emitted so far.

=h reset_counts write-only

Resets "count" to 0.

=h filename read-only

Returns the filename.

=a

FromDump, FromDevice.u, ToDevice.u, tcpdump(1) */

class ToDump : public Element { public:

    ToDump() CLICK_COLD;
    ~ToDump() CLICK_COLD;

    const char *class_name() const	{ return "ToDump"; }
    const char *port_count() const	{ return "1/0-1"; }
    const char *flags() const		{ return "S2"; }

    // configure after FromDevice and FromDump
    int configure_phase() const		{ return CONFIGURE_PHASE_DEFAULT+100; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    ToDump *hotswap_element() const;
    void take_state(Element *, ErrorHandler *);

    void push(int, Packet *);
    Packet *pull(int);
    bool run_task(Task *);

  private:

    String _filename;
    FILE *_fp;
    unsigned _snaplen;
    int _linktype;
    bool _active;
    bool _extra_length;
    bool _unbuffered;
    bool _nano;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;

    Task _task;
    NotifierSignal _signal;
    Element **_use_encap_from;

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;
    void write_packet(Packet *);

};

CLICK_ENDDECLS
#endif
