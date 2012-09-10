// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMTCPDUMP_HH
#define CLICK_FROMTCPDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <click/ipflowid.hh>
#include <click/hashtable.hh>
#include <clicknet/tcp.h>
#include <click/fromfile.hh>
#include "ipsumdumpinfo.hh"
CLICK_DECLS

/*
=c

FromTcpdump(FILENAME [, I<KEYWORDS>])

=s traces

reads packets from an ASCII tcpdump output file

=d

Reads textual TCP/IP packet descriptors from an ASCII tcpdump(1) output file,
then creates packets resembling those descriptors and pushes them out the
output. Optionally stops the driver when there are no more packets.

The file may be compressed with gzip(1) or bzip2(1); FromTcpdump will
run zcat(1) or bzcat(1) to uncompress it.

FromTcpdump reads from the file named FILENAME unless FILENAME is a
single dash `C<->', in which case it reads from the standard input. It will
not uncompress the standard input, however.

FromTcpdump doesn't parse many of the relevant parts of the file. It handles
fragments badly, for example. Mostly it just does TCP and some rudimentary
UDP.

Keyword arguments are:

=over 8

=item STOP

Boolean. If true, then FromTcpdump will ask the router to stop when it
is done reading. Default is false.

=item ACTIVE

Boolean. If false, then FromTcpdump will not emit packets (until the
`C<active>' handler is written). Default is true.

=item ZERO

Boolean. Determines the contents of packet data not set by the dump. If true
E<lparen>the default), this data is zero. If false, it is random garbage.

=item CHECKSUM

Boolean. If true, then output packets' IP, TCP, and UDP checksums are set. If
false (the default), the checksum fields contain random garbage.

=item SAMPLE

Unsigned real number between 0 and 1. FromTcpdump will output each
packet with probability SAMPLE. Default is 1. FromTcpdump uses
fixed-point arithmetic, so the actual sampling probability may differ
substantially from the requested sampling probability. Use the
C<sampling_prob> handler to find out the actual probability. If MULTIPACKET is
true, then the sampling probability applies separately to the multiple packets
generated per record.

=back

Only available in user-level processes.

=n

FromTcpdump is a notifier signal, active when the element is active and
the dump contains more packets.

tcpdump(1)'s binary output is generally much better than the output of
FromTcpdump. Unfortunately, some people just throw it away.

=h sampling_prob read-only

Returns the sampling probability (see the SAMPLE keyword argument).

=h active read/write

Value is a Boolean.

=h encap read-only

Returns `IP'. Useful for ToDump's USE_ENCAP_FROM option.

=h filesize read-only

Returns the length of the FromTcpdump file, in bytes, or "-" if that
length cannot be determined.

=h filepos read-only

Returns FromTcpdump's position in the file, in bytes.

=h stop write-only

When written, sets `active' to false and stops the driver.

=a

tcpdump(1), FromDump, FromIPSummaryDump */

class FromTcpdump : public Element { public:

    FromTcpdump() CLICK_COLD;
    ~FromTcpdump() CLICK_COLD;

    const char *class_name() const	{ return "FromTcpdump"; }
    const char *port_count() const	{ return PORTS_0_1; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    bool run_task(Task *);
    Packet *pull(int);

  private:

    enum { SAMPLING_SHIFT = 28 };

    FromFile _ff;

    uint32_t _sampling_prob;

    bool _stop : 1;
    bool _format_complaint : 1;
    bool _zero : 1;
    bool _checksum : 1;
    bool _active : 1;
    bool _dead : 1;

    struct FlowRecord {
	tcp_seq_t init_seq[2];
	tcp_seq_t last_seq[2];
	inline FlowRecord() { init_seq[0] = init_seq[1] = 0; }
    };
    HashTable<IPFlowID, FlowRecord> _tcp_map;
    int _absolute_seq;

    Task _task;
    ActiveNotifier _notifier;

    Packet *read_packet(ErrorHandler *);
    const char *read_tcp_line(WritablePacket *&, const char *begin, const char *end, int *data_len);
    const char *read_udp_line(WritablePacket *&, const char *begin, const char *end, int *data_len);

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
