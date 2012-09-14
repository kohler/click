#ifndef CLICK_ARPPRINT_HH
#define CLICK_ARPPRINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

ARPPrint([TAG, I<KEYWORDS>])

=s arp

pretty-prints ARP packets a la tcpdump

=d

Expects ARP packets as input.

Prints out ARP packets in a human-readable tcpdump-like format, preceded by
the TAG text.

Keyword arguments are:

=over 2

=item TIMESTAMP

Boolean. Determines whether to print each packet's timestamp in seconds since
1970. Default is true.

=item ETHER

Boolean.  Determines whether to print each packet's Ethernet addresses.
Default is false.

=item OUTFILE

String. Only available at user level. PrintV<> information to the file specified
by OUTFILE instead of standard error.

=item ACTIVE

Boolean.  If false, don't print messages.  Default is true.

=back

=h active read/write

Returns or sets the ACTIVE parameter.

=a Print, CheckARPHeader */

class ARPPrint : public Element { public:

    ARPPrint() CLICK_COLD;
    ~ARPPrint() CLICK_COLD;

    const char *class_name() const		{ return "ARPPrint"; }
    const char *port_count() const		{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    Packet *simple_action(Packet *);

 private:

    String _label;
    bool _print_timestamp;
    bool _print_ether;
    bool _active;

#if CLICK_USERLEVEL
    String _outfilename;
    FILE *_outfile;
#endif
    ErrorHandler *_errh;

};

CLICK_ENDDECLS
#endif
