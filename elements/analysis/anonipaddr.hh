// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ANONIPADDR_HH
#define CLICK_ANONIPADDR_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AnonymizeIPAddr

=s ip

anonymizes top-level IP addresses in passing packets

=d

AnonymizeIPAddr anonymizes the source and destination IP addresses in passing
IPv4 packets. (Packets must have IP header annotations.) The anonymization
transformation is prefix-preserving: If two input addresses shared the same
p-bit prefix, then the corresponding output addresses also share the same
p-bit prefix. AnonymizeIPAddr was based on Greg Minshall's tcpdpriv(1); see
L<http://ita.ee.lbl.gov/html/contrib/tcpdpriv.html|http://ita.ee.lbl.gov/html/contrib/tcpdpriv.html>.

The special IP addresses 0.0.0.0 and 255.255.255.255 are always mapped to
themselves, independent of any other mapping.

AnonymizeIPAddr also incrementally updates the IP header checksum, so the new
header is correct iff the old header was correct.

AnonymizeIPAddr only manipulates the IP header pointed to by the IP header
annotation. This differs from tcpdpriv, which also anonymizes addresses on
encapsulated IP headers for protocol 4 (ipip).

Keyword arguments are:

=over 8

=item CLASS

Integer. Preserve some "class" information from input IP addresses. If CLASS
is 1, then class A is preserved: an output address is in class A if and only
if the corresponding input address was in class A. If CLASS is 2, then class B
is preserved as well. CLASS 3 preserves classes A, B, and C, and CLASS 4
preserves classes A, B, C, and D. The CLASS flag works by preserving leading
one bits; higher CLASSes, up to 32, preserve more one bits. Default CLASS is 0
E<lparen>no preservation).

=item PRESERVE_8

Space-separated list of integers. Preserve the listed 8-bit prefixes. For
example, with 'PRESERVE_8 18', an output address is in the network 18.0.0.0/8
if and only if the input address was in that network. Default is empty.

In a prefix-preserving anonymization, PRESERVE_8 introduces structure into
nearby 8-bit prefixes. For example, 'PRESERVE_8 18' also maps net 19 to net
19: nets 18 and 19 share their top 7 bits, so
because of prefix preservation, net 19 must map to itself. Other nearby
networks are permuted: nets 16 and 17, for example, must map to themselves or
to each other. Here is the complete list:

    Input nets  map to  Output nets
       0-15      ...       0-15
      16-17      ...      16-17
       18        ...       18
       19        ...       19
      20-23      ...      20-23
      24-31      ...      24-31
      32-63      ...      32-63
      64-127     ...      64-127
     128-255     ...     128-255

=back

=n

AnonymizeIPAddr's anonymization corresponds to tcpdpriv's -A50 option.

Prefix-preserving anonymization is not foolproof. The L<http://ita.ee.lbl.gov/html/contrib/tcpdpriv.html|tcpdpriv distribution> contains a paper describing the possible attack. Tatu Ylonen closes that document by saying: "If you are
very concerned about leaking your network topology, I would not
recommend giving out trace information privatized with the I<-A50>
option.  I wouldn't expect this to be the case for most organizations."

=h CLICK_LLRPC_MAP_IPADDRESS llrpc

Argument is a pointer to an IP address. An IP address is read from that
location; the corresponding anonymized IP address is then stored into that
location.

=a

tcpdpriv(1) */

class AnonymizeIPAddr : public Element { public:

    AnonymizeIPAddr() CLICK_COLD;
    ~AnonymizeIPAddr() CLICK_COLD;

    const char *class_name() const	{ return "AnonymizeIPAddr"; }
    const char *port_count() const	{ return PORTS_1_1X2; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    Packet *simple_action(Packet *);

    int llrpc(unsigned, void *);

  private:

    struct Node {
	uint32_t input;
	uint32_t output;
	Node *child[2];
    };

    Node *_root;
    Node *_free;
    Vector<Node *> _blocks;
    Node _special_nodes[2];

    int _preserve_class;
    Vector<uint32_t> _preserve_8;

    Node *new_node();
    Node *new_node_block();
    void free_node(Node *);

    uint32_t make_output(uint32_t, int) const;
    Node *make_peer(uint32_t, Node *);
    Node *find_node(uint32_t);
    inline uint32_t anonymize_addr(uint32_t);

    void handle_icmp(WritablePacket *);

};

inline AnonymizeIPAddr::Node *
AnonymizeIPAddr::new_node()
{
    if (_free) {
	Node *n = _free;
	_free = n->child[0];
	return n;
    } else
	return new_node_block();
}

inline void
AnonymizeIPAddr::free_node(Node *n)
{
    n->child[0] = _free;
    _free = n;
}

CLICK_ENDDECLS
#endif
