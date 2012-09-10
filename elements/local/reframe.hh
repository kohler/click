#ifndef CLICK_REFRAME_HH
#define CLICK_REFRAME_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Reframe([I<keywords> FOFF, FLEN, NTOH, MUL, ALIGN, ADD])

=s packet framing

reframes a fragmented stream of packets

=d

Queues incoming packet fragments until a full Ethernet, IP, or
fixed-size frame can be built, then emits it. Does not set packet
annotations. Useful for reframing a fragmented stream of packets
emitted by a userlevel element like Socket. Assumes that the stream is
either continuous, or that frames are separated by aligned and/or
fixed-size padding.

The length of each frame should be encoded in a FLEN (1, 2, or 4) byte
field located FOFF bytes into each frame. The encoded length is
assumed to include FOFF plus FLEN. If the field is encoded in network
order, specify the keyword NTOH. Specify the byte multiplier that
should be used in the keyword MUL, the alignment within the byte
stream (if any) of each frame in the keyword ALIGN, and any additional
bytes that should be added to the result in the keyword ADD.

If FLEN is 0, all keywords except ADD are ignored and the length of
each frame is assumed to be fixed at ADD bytes.

For example, to reframe a fragment stream of IP packets (total length
in bytes is encoded 2 bytes into the frame as a 2-byte variable in
network order):

  ... -> Reframe(FOFF 2, FLEN 2, NTOH 1, MUL 1, ALIGN 1, ADD 0) -> ...

Or since these are the keyword defaults:

  ... -> Reframe -> ...

To reframe a fragment stream of IP-in-Ethernet packets:

  ... -> Reframe(FOFF 16, FLEN 2, ADD 14) -> ...

To reframe a fragment stream of fixed-size 100 byte packets:

  ... -> Reframe(FLEN 0, ADD 100) -> ...

Keyword arguments are:

=over 8

=item FOFF

Integer. Specifies the byte offset of the encoded length field in each
frame in the fragment stream. Default is 2 (IP).

=item FLEN

Integer. Specifies the length of the encoded length field in each
frame in the fragment stream. Default is 2 (IP).

=item NTOH

Boolean. When true, Reframe will assume that the length field is
encoded in network order in each frame in the fragment stream. Default
is true (IP).

=item MUL

Integer. Specifies the number of bytes per unit of encoded
length. Default is 1 (IP).

=item ALIGN

Integer. Specifies the byte alignment of each frame in the fragment
stream. For example, a byte alignment of 4 would ensure that each
frame began and ended at a byte offset (from the start of the stream)
that was divisible by 4. Default is 1 (IP).

=item ADD

Integer. Specifies the number of additional bytes that should be added
to each frame in the fragment stream. Default is 0 (IP).

=back

=e

// Reframe a fragmented stream of tunnelled IP frames
Socket(TCP, 0.0.0.0, 12345) -> Reframe -> ...

// Reframe a fragmented stream of tunnelled IP-in-GRE frames. Plain
// GRE headers are 4 bytes long and do not include a payload length
// field, so we must reach 4 + 2 = 6 bytes into each frame to reach
// the IP length field, and add 4 bytes to account for the GRE header.
Socket(TCP, 0.0.0.0, 12345) -> Reframe(FOFF 6, ADD 4) -> StripGREHeader ...

// A useless 1000 byte latency bucket
... -> Reframe(FLEN 0, ADD 1000) -> ...

=a Socket */

class Reframe : public Element { public:

    Reframe() CLICK_COLD;
    ~Reframe() CLICK_COLD;

    const char *class_name() const	{ return "Reframe"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return "h/a"; }

  int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;

  void push(int, Packet*);
  Packet * pull(int);

 private:

  int _foff;
  int _flen;
  bool _ntoh;
  int _mul;
  int _align;
  int _add;

  WritablePacket *_header;
  Packet *_qhead, *_qtail;
  int _have, _need;

  void reframe(void);
};

CLICK_ENDDECLS
#endif
