// -*- c-basic-offset: 4 -*-
#ifndef CLICK_IPREASSEMBLER_HH
#define CLICK_IPREASSEMBLER_HH

/*
 * =c
 * IPReassembler()
 * =s IP
 * Reassembles fragmented IP packets
 * =d
 * Expects IP packets as input to port 0. If input packets are 
 * fragments, IPReassembler holds them until it has enough fragments
 * to recreate a complete packet. If a set of fragments making a single
 * packet is incomplete and dormant for 30 seconds, the fragments are
 * dropped. When a complete packet is constructed, it is pushed onto
 * output 0.
 *
 * =a IPFragmenter
 */

#include <click/element.hh>
#include <click/glue.hh>
#include <click/click_ip.h>
#include <click/timer.hh>

class IPReassembler : public Element { public:
  
    IPReassembler();
    ~IPReassembler();

    const char *class_name() const	{ return "IPReassembler"; }
    const char *processing() const	{ return AGNOSTIC; }
    IPReassembler *clone() const	{ return new IPReassembler; }

    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    int check(ErrorHandler * = 0);
    
    Packet *simple_action(Packet *);

    struct PacketLink {
	uint32_t padding[2];	// preserve paint annotation
	Packet *next;
	Packet *bucket_next;
    };

  private:

    enum { IPFRAG_HIGH_THRESH = 256 * 1024,
	   IPFRAG_LOW_THRESH  = 192 * 1024,
	   EXPIRE_TIMEOUT = 30, // seconds
	   EXPIRE_TIMER_INTERVAL_MS = 10000 }; // ms

    int _mem_used;
    enum { NMAP = 256 };
    Packet *_map[NMAP];

    Timer _expire_timer;

    static inline int bucketno(const click_ip *);
    static inline bool same_segment(const click_ip *, const click_ip *);
    
    Packet *find_queue(Packet *, Packet ***);
    static void clean_queue(Packet *, Packet **);
    Packet *emit_whole_packet(Packet *, Packet **, Packet *, Packet *);
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

#endif
