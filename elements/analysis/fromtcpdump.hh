// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMTCPDUMP_HH
#define CLICK_FROMTCPDUMP_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <click/ipflowid.hh>
#include <click/hashmap.hh>
#include <clicknet/tcp.h>
#include "ipsumdumpinfo.hh"
CLICK_DECLS

/*
=c

FromTcpdump(FILENAME [, I<KEYWORDS>])

=s analysis

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
fragments badly, for example, and doesn't parse IP options. Mostly it just
does TCP and some rudimentary UDP.

Keyword arguments are:

=over 8

=item STOP

Boolean. If true, then FromTcpdump will ask the router to stop when it
is done reading. Default is false.

=item ACTIVE

Boolean. If false, then FromTcpdump will not emit packets (until the
`C<active>' handler is written). Default is true.

=item ZERO

Boolean. Determines the contents of packet data not set by the dump. If true,
this data is zero. If false (the default), this data is random garbage.

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

class FromTcpdump : public Element, public IPSummaryDumpInfo { public:

    FromTcpdump();
    ~FromTcpdump();

    const char *class_name() const	{ return "FromTcpdump"; }
    const char *processing() const	{ return AGNOSTIC; }
    FromTcpdump *clone() const		{ return new FromTcpdump; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    bool run_task();
    Packet *pull(int);
    
  private:

    enum { BUFFER_SIZE = 32768, SAMPLING_SHIFT = 28 };
    
    int _fd;
    char *_buffer;
    int _pos;
    int _len;
    int _buffer_len;
    int _save_char;
    int _lineno;

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
    HashMap<IPFlowID, FlowRecord> _tcp_map;
    int _absolute_seq;

    Task _task;
    ActiveNotifier _notifier;

    String _filename;
    FILE *_pipe;
    off_t _file_offset;

    int error_helper(ErrorHandler *, const char *);
    int read_buffer(ErrorHandler *);
    int read_line(String &, ErrorHandler *);

    Packet *read_packet(ErrorHandler *);
    const char *read_tcp_line(WritablePacket *&, const String &, const char *s, int *data_len);

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);
    
};

CLICK_ENDDECLS
#endif
