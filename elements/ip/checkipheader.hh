#ifndef CHECKIPHEADER_HH
#define CHECKIPHEADER_HH

/*
=c

CheckIPHeader([BADADDRS, OFFSET, I<KEYWORDS>])

=s IP, checking

checks IP header

=d

Input packets should have IP headers starting OFFSET bytes in. Default OFFSET
is zero. Checks that the packet's length is reasonable, and that the IP
version, header length, length, and checksum fields are valid. Checks that the
IP source address is a legal unicast address -- that is, that it is not
0.0.0.0 or 255.255.255.255, or a local broadcast addresses in BADADDRS.
Shortens packets to the IP length, if the IP length is shorter than the
nominal packet length (due to Ethernet padding, for example). Also sets the
destination IP address annotation to the actual destination IP address, unless
that annotation was already set.

CheckIPHeader emits valid packets on output 0. Invalid packets are pushed out
on output 1, unless output 1 was unused; if so, drops invalid packets.

The BADADDRS argument is a space-separated list of IP addresses that are
not to be tolerated as source addresses. Usually consists of the subnet
broadcast addresses on each interface (RFC1812 5.3.7).

CheckIPHeader prints a message to the console the first time it encounters an
incorrect IP packet (but see VERBOSE below).

Keyword arguments are:

=over 5

=item VERBOSE

Boolean. If it is true, then a message will be printed for every erroneous
packet, rather than just the first. False by default.

=item DETAILS

Boolean. If it is true, then CheckIPHeader will maintain detailed counts of
how many packets were dropped for each possible reason, accessible through the
C<drop_details> handler. False by default.

=back

=h drops read-only

Returns the number of incorrect packets CheckIPHeader has seen.

=h drop_details read-only

Returns a text file showing how many erroneous packets CheckIPHeader has seen,
subdivided by error. Only available if the DETAILS keyword argument was true.

=a CheckIPHeader2, MarkIPHeader, SetIPChecksum, StripIPHeader */

#include <click/element.hh>
#include <click/atomic.hh>

class CheckIPHeader : public Element { public:

  CheckIPHeader();
  ~CheckIPHeader();
  
  const char *class_name() const		{ return "CheckIPHeader"; }
  const char *processing() const		{ return "a/ah"; }
  CheckIPHeader *clone() const;
  
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);

 private:
  
  int _n_bad_src;
  uint32_t *_bad_src; // array of illegal IP src addresses.
  unsigned _offset;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned : 1;
#endif
  bool _verbose : 1;
  uatomic32_t _drops;
  uatomic32_t *_reason_drops;

  enum Reason {
    MINISCULE_PACKET,
    BAD_VERSION,
    BAD_HLEN,
    BAD_IP_LEN,
    BAD_CHECKSUM,
    BAD_SADDR,
    NREASONS
  };
  static const char *reason_texts[NREASONS];
  
  Packet *drop(Reason, Packet *);
  static String read_handler(Element *, void *);

  friend class CheckIPHeader2;
  
};

#endif
