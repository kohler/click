#ifndef CLICK_OLDIPREASSEMBLER_HH
#define CLICK_OLDIPREASSEMBLER_HH

/*
 * =c
 * OldIPReassembler()
 * =s IP
 * Reassembles fragmented IP packets
 * =d
 * Expects IP packets as input to port 0. If input packets are 
 * fragments, OldIPReassembler holds them until it has enough fragments
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

class OldIPReassembler : public Element {

 public:
  
  OldIPReassembler();
  ~OldIPReassembler();
  
  const char *class_name() const		{ return "OldIPReassembler"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  OldIPReassembler *clone() const;

  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);

  Packet* simple_action(Packet *);

  struct FragEntry {
    int original_size;
    int offset;
    int end;
    int len;
    Packet *p;               // pointer to packet object
    unsigned char const *ptr;// pointer to actual data
    struct FragEntry *prev;
    struct FragEntry *next;
  };

  struct IPQueue {
    click_ip const *iph;
    struct FragEntry *frags;
    int len;                 // total length of original data (not including header)
    short ihlen;             // length of ip header
    struct IPQueue **pprev;
    struct IPQueue *next;
    int last_touched_jiffy;
  };


  int hashfn(unsigned short id, unsigned src, unsigned dst, unsigned char prot); 

private:
  
  enum { IPFRAG_HIGH_THRESH = 256 * 1024,
	 IPFRAG_LOW_THRESH  = 192 * 1024,
	 EXPIRE_TIMEOUT = 30, // seconds
	 EXPIRE_TIMER_INTERVAL_MS = 3000 }; // ms
  
  int _mem_used;
  enum { NMAP = 256 };
  IPQueue *_map[NMAP];

  Timer _expire_timer;

  struct OldIPReassembler::IPQueue *queue_find(const struct click_ip *iph);
  int  queue_done(struct IPQueue *q);  
  Packet *queue_glue(struct IPQueue *q);
  void queue_free(struct IPQueue *qp, int free_first);
  void queue_evictor(void);

  struct OldIPReassembler::FragEntry *frag_create(int offset, int end, Packet *p);
  struct OldIPReassembler::IPQueue *queue_create(const click_ip *ipheader);
  
  static void expire_hook(Timer*, void *thunk);

};

#endif






















