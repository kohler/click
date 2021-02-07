// -*- c-basic-offset: 4 -*-
#ifndef CLICK_IPREASSEMBLER_HH
#define CLICK_IPREASSEMBLER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

IPReassembler([I<KEYWORDS>])

=s ip

Reassembles fragmented IP packets

=d

Expects IP packets as input to port 0. If input packets are fragments,
IPReassembler holds them until it has enough fragments to recreate a complete
packet. When a complete packet is constructed, it is emitted onto output 0. If
a set of fragments making a single packet is incomplete and dormant for 30
seconds, the fragments are generally dropped. If IPReassembler has two
outputs, however, a single packet containing all the received fragments at
their proper offsets is pushed onto output 1.

IPReassembler's memory usage is bounded. When memory consumption rises above
HIMEM bytes, IPReassembler throws away old fragments until memory consumption
drops below 3/4*HIMEM bytes. Default HIMEM is 256K.

Output packets have the same MAC header as the fragment that contains
offset 0.  Other than that, input MAC headers are ignored.

The IPREASSEMBLER annotation area is used to store packet metadata about
packets in the process of reassembly.  On emitted reassembled packets,
this annotation area is set to 0.

Keyword arguments are:

=over 8

=item HIMEM

The upper bound for memory consumption, in bytes. Default is 256K.

=item MAX_MTU_ANNO

Optional. A 2 byte annotation that will be filled with the maximum size of any
one fragment of this packet. If no reassembly is required, then the annotation
is unchanged.

=back

=n

You may want to attach an C<ICMPError(ADDR, timeexceeded, reassembly)> to the
second output.

IPReassembler destroys its input packets' "next packet" annotations.

=a IPFragmenter */

class IPReassembler : public Element { public:

    IPReassembler() CLICK_COLD;
    ~IPReassembler() CLICK_COLD;

    const char *class_name() const	{ return "IPReassembler"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    int check(ErrorHandler * = 0);

    Packet *simple_action(Packet *);

    void add_handlers() CLICK_COLD;

    struct ChunkLink {
	uint16_t off;
	uint16_t lastoff;
    } __attribute__((packed));

  private:

    enum { REAP_TIMEOUT = 30, // seconds
	   REAP_INTERVAL = 10, // seconds
	   IPH_MEM_USED = 40 };

    enum { NMAP = 256 };
    WritablePacket *_map[NMAP];

    int _reap_time;

    uint32_t _stat_frags_seen;
    uint32_t _stat_good_assem;
    uint32_t _stat_failed_assem;
    uint32_t _stat_bad_pkts;

    uint32_t _mem_used;
    uint32_t _mem_high_thresh;	// defaults to 256K
    uint32_t _mem_low_thresh;	// defaults to 3/4 * _mem_high_thresh
    int8_t _mtu_anno;

    static inline int bucketno(const click_ip *);
    static inline bool same_segment(const click_ip *, const click_ip *);
    static String debug_dump(Element *e, void *);

    WritablePacket *find_queue(Packet *, WritablePacket ***);
    void make_queue(Packet *, WritablePacket **);
    static ChunkLink *next_chunk(WritablePacket *, ChunkLink *);
    Packet *emit_whole_packet(WritablePacket *, WritablePacket **, Packet *);
    void reap_overfull(int);
    void reap(int);
    static void check_error(ErrorHandler *, int, const Packet *, const char *, ...);

};


inline int
IPReassembler::bucketno(const click_ip *h)
{
    return (h->ip_id % NMAP);
}

inline bool
IPReassembler::same_segment(const click_ip *h, const click_ip *h2)
{
    return h->ip_id == h2->ip_id && h->ip_p == h2->ip_p
	&& h->ip_src.s_addr == h2->ip_src.s_addr
	&& h->ip_dst.s_addr == h2->ip_dst.s_addr;
}

CLICK_ENDDECLS
#endif
