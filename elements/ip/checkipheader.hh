#ifndef CLICK_CHECKIPHEADER_HH
#define CLICK_CHECKIPHEADER_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS
class Args;

/*
=c

CheckIPHeader([OFFSET, I<keywords> OFFSET, INTERFACES, BADSRC,
                       GOODDST, CHECKSUM, VERBOSE, DETAILS])

=s ip

checks IP header

=d

Input packets should have IP headers starting OFFSET bytes in. Default OFFSET
is zero. Checks that the packet's length is reasonable, and that the IP
version, header length, length, and checksum fields are valid. Checks that the
IP source address is a legal unicast address, or that the packet is destined
for one of this machine's addresses (see below). Shortens packets to the IP
length, if the IP length is shorter than the nominal packet length (due to
Ethernet padding, for example). Also sets the destination IP address
annotation to the actual destination IP address.

CheckIPHeader emits valid packets on output 0. Invalid packets are pushed out
on output 1, unless output 1 was unused; if so, drops invalid packets.

CheckIPHeader checks packets' source addresses for validity if one or more of
the INTERFACES, BADSRC, and GOODDST keyword arguments are supplied (RFC1812
5.3.7).

CheckIPHeader prints a message to the console the first time it encounters an
incorrect IP packet (but see VERBOSE below).

Keyword arguments are:

=over 5

=item CHECKSUM

Boolean. If true, then check each packet's checksum for validity; if false, do
not check the checksum. Default is true.

=item OFFSET

Unsigned integer. Byte position at which the IP header begins. Default is 0.

=item BADSRC

Space-separated list of IP addresses. CheckIPHeader will drop packets whose
source address is on this list (but see GOODDST below). Default is empty.

=item GOODDST

Space-separated list of IP addresses. CheckIPHeader exempts packets whose
destination address is on this list from BADSRC processing. Default is empty.

=item INTERFACES

Space-separated list of IP addresses with network prefixes, meant to represent
this router's interface addresses. This argument specifies both BADSRC and
GOODDST. Specifically, the argument "INTERFACES 18.26.4.9/24 18.32.9.44/28"
acts like a BADSRC of "18.26.4.255 18.32.9.47 0.0.0.0 255.255.255.255" -- the
set of broadcast addresses for this router -- and a GOODDST of "18.26.4.9
18.32.9.44". Default is not given.

=item VERBOSE

Boolean. If it is true, then a message will be printed for every erroneous
packet, rather than just the first. False by default.

=item DETAILS

Boolean. If it is true, then CheckIPHeader will maintain detailed counts of
how many packets were dropped for each possible reason, accessible through the
C<drop_details> handler. False by default.

=back

=n

CheckIPHeader supports an old configuration syntax:

CheckIPHeader([BADSRC2, OFFSET, I<keywords>])

The BADSRC2 argument behaves like the BADSRC keyword, except that if you use
this syntax, 0.0.0.0 and 255.255.255.255 considered bad addresses in addition
to those explicitly in the list. This syntax is deprecated and should not be
used in new configurations.

=h drops read-only

Returns the number of incorrect packets CheckIPHeader has seen.

=h drop_details read-only

Returns a text file showing how many erroneous packets CheckIPHeader has seen,
subdivided by error. Only available if the DETAILS keyword argument was true.

=a CheckIPHeader2, MarkIPHeader, SetIPChecksum, StripIPHeader,
CheckTCPHeader, CheckUDPHeader, CheckICMPHeader */

class CheckIPHeader : public Element { public:

  CheckIPHeader() CLICK_COLD;
  ~CheckIPHeader() CLICK_COLD;

  const char *class_name() const		{ return "CheckIPHeader"; }
  const char *port_count() const		{ return PORTS_1_1X2; }
  const char *processing() const		{ return PROCESSING_A_AH; }
  const char *flags() const			{ return "A"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

  Packet *simple_action(Packet *);

  struct OldBadSrcArg {
      static bool parse(const String &str, Vector<IPAddress> &result,
			Args &args);
  };

  struct InterfacesArg {
      static bool parse(const String &str, Vector<IPAddress> &result_bad_src,
			Vector<IPAddress> &result_good_dst, Args &args);
  };

 private:

  unsigned _offset;

  Vector<IPAddress> _bad_src;	// array of illegal IP src addresses

  bool _checksum;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned;
#endif
  bool _verbose;

  Vector<IPAddress> _good_dst;	// array of IP dst addrs for which _bad_src
				// does not apply

  atomic_uint32_t _drops;
  atomic_uint32_t *_reason_drops;

  enum Reason {
    MINISCULE_PACKET,
    BAD_VERSION,
    BAD_HLEN,
    BAD_IP_LEN,
    BAD_CHECKSUM,
    BAD_SADDR,
    NREASONS
  };
  static const char * const reason_texts[NREASONS];

  Packet *drop(Reason, Packet *);
  static String read_handler(Element *, void *) CLICK_COLD;

  friend class CheckIPHeader2;

};

CLICK_ENDDECLS
#endif
