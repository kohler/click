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

=s IP

Reassembles fragmented IP packets

=d

Expects IP packets as input to port 0. If input packets are 
fragments, IPReassembler holds them until it has enough fragments
to recreate a complete packet. If a set of fragments making a single
packet is incomplete and dormant for 30 seconds, the fragments are
dropped. When a complete packet is constructed, it is pushed onto
output 0.

IPReassembler's memory usage is bounded. When memory consumption rises above
HIMEM bytes, IPReassembler throws away old fragments until memory consumption
drops below 3/4*HIMEM bytes. Default HIMEM is 256K.

Keyword arguments are:

=over 8

=item HIMEM

The upper bound for memory consumption, in bytes. Default is 256K.

=back

=a IPFragmenter */

class IPReassembler : public Element { public:
  
    IPReassembler();
    ~IPReassembler();

    const char *class_name() const	{ return "IPReassembler"; }
    const char *processing() const	{ return AGNOSTIC; }
    IPReassembler *clone() const	{ return new IPReassembler; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    int check(ErrorHandler * = 0);
    
    Packet *simple_action(Packet *);

    struct ChunkLink {
	uint16_t off;
	uint16_t lastoff;
    };
    struct PacketInfo {
	uint32_t padding[3];	// preserve paint annotation
	ChunkLink chunk;
    };

  private:

    enum { EXPIRE_TIMEOUT = 30, // seconds
	   EXPIRE_TIMER_INTERVAL_MS = 10000, // ms
	   IPH_MEM_USED = 40 };

    enum { NMAP = 256 };
    WritablePacket *_map[NMAP];

    Timer _expire_timer;
    uint32_t _mem_used;
    uint32_t _mem_high_thresh;	// defaults to 256K
    uint32_t _mem_low_thresh;	// defaults to 3/4 * _mem_high_thresh

    static inline int bucketno(const click_ip *);
    static inline bool same_segment(const click_ip *, const click_ip *);
    
    WritablePacket *find_queue(Packet *, WritablePacket ***);
    void make_queue(Packet *, WritablePacket **);
    static ChunkLink *next_chunk(WritablePacket *, ChunkLink *);
    Packet *emit_whole_packet(WritablePacket *, WritablePacket **, Packet *);
    void garbage_collect();
    static void check_error(ErrorHandler *, int, const Packet *, const char *, ...);
    static void expire_hook(Timer *, void *);

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
