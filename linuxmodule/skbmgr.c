#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <net/dst.h>
#include <linux/if_packet.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <click/skbmgr.h>

#define DEBUG_SKBMGR 0

// number of buckets corresponds to how many different
// buffer sizes we want to accomodate. right now, we
// want to keep packet sizes of >=128, >=256, >=512,
// >=900, >=1600, and >=2048 bytes.
#define BUCKETS 6

struct recycled_skb_pool {
  struct {
    unsigned volatile lock;
    unsigned head;
    unsigned tail;
#define SKB_RECYCLED 61
    struct sk_buff *skbs[SKB_RECYCLED];
  } buckets[BUCKETS];
  int last_producer;
  int consumers;
#if DEBUG_SKBMGR
  int allocated;
  int freed;
  int recycle_freed;
  int recycle_allocated;
  int pad[2];
#else
  int pad[6];
#endif
};

#define SKBMGR_DEF_TAILSZ 64
#define SKBMGR_DEF_HEADSZ 64

static struct recycled_skb_pool pools[NR_CPUS];

#define SKBQ_NEXT(x) (x+1 != SKB_RECYCLED ? x+1 : 0)
#define SKBQ_SIZE(cpu,bucket) \
  (pools[cpu].buckets[bucket].tail >= pools[cpu].buckets[bucket].head \
   ? pools[cpu].buckets[bucket].tail - pools[cpu].buckets[bucket].head \
   : pools[cpu].buckets[bucket].tail + SKB_RECYCLED \
     - pools[cpu].buckets[bucket].head)


void
skbmgr_init(void)
{
  int i, j, k;
  for(i=0; i < NR_CPUS; i++) {
    for(j=0; j<BUCKETS; j++) {
      pools[i].buckets[j].lock = 0;
      pools[i].buckets[j].head = 0;
      pools[i].buckets[j].tail = 0;
      for(k=0; k<SKB_RECYCLED; k++)
        pools[i].buckets[j].skbs[k] = 0;
    }
    pools[i].last_producer = -1;
    pools[i].consumers = 0;
#if DEBUG_SKBMGR
    pools[i].recycle_freed = 0;
    pools[i].freed = 0;
    pools[i].recycle_allocated = 0;
    pools[i].allocated = 0;
#endif
  }
}

void
skbmgr_cleanup(void)
{
  int i,j,k;
  for(i=0; i < NR_CPUS; i++) {
    for(j=0; j<BUCKETS; j++) {
      pools[i].buckets[j].lock = 0;
      pools[i].buckets[j].head = 0;
      pools[i].buckets[j].tail = 0;
      for(k=0; k<SKB_RECYCLED; k++) {
	if (pools[i].buckets[j].skbs[k]) {
          struct sk_buff *skb = pools[i].buckets[j].skbs[k];
	  kfree_skb(skb);
	  pools[i].buckets[j].skbs[k] = 0;
	}
      }
    }
#if DEBUG_SKBMGR
    if (pools[i].freed > 0 || pools[i].allocated > 0 || pools[i].consumers > 0)
      printk("pool %d: %d/%d freed %d/%d allocated, %d consumers\n", i, 
	     pools[i].freed, pools[i].recycle_freed,
	     pools[i].allocated, pools[i].recycle_allocated, 
	     pools[i].consumers);
#endif
  }
}


#define APPEND_TO_LIST(__head, __tail, __skb) \
  if (__head == 0) { 		\
    __head = __skb;    		\
    __tail = __skb;		\
    __tail->next = NULL;	\
  } else {			\
    __tail->next = __skb;	\
    __skb->next = NULL;		\
    __tail = __skb;		\
  }

static inline int
put_size_into_bucket(unsigned size)
{
  return (size>=2048) ? 5 :
          ((size>=1600) ? 4 :
	   ((size>=900) ? 3 :
	    ((size>=512) ? 2 :
	     ((size>=256) ? 1 :
	      ((size>=128) ? 0:
	       -1)))));
}

static inline int
get_allocation_size(unsigned size)
{
  return (size>2048) ? size :
          ((size>1600) ? 2048 :
	   ((size>900) ? 1600 :
	    ((size>512) ? 900 :
	     ((size>256) ? 512 :
	      ((size>128) ? 256 : 128)))));
}

static inline int
get_size_outof_bucket(unsigned size)
{
  return (size>2048) ? -1 :
          ((size>1600) ? 5 :
	   ((size>900) ? 4 :
	    ((size>512) ? 3 :
	     ((size>256) ? 2 :
	      ((size>128) ? 1 : 0)))));
}


static inline void 
skb_recycled_init_fast(struct sk_buff *skb)
{
  // i am only resetting the fields that need to be set.
  // for example, users should already be at 1, so is
  // datarefp (expensive to set).
  if (!(skb->pkt_type & PACKET_CLEAN)) {
    dst_release(skb->dst);
    if (skb->destructor) {
      skb->destructor(skb);
      skb->destructor = NULL;
    }
    skb->pkt_bridged = 0;
    skb->prev = NULL;
    skb->list = NULL;
    skb->sk = NULL;
    skb->security = 0;
    skb->priority = 0;
  }
  skb->pkt_type = PACKET_HOST;
  // XXX - what should we do here
  // memset(skb->cb, 0, sizeof(skb->cb));
}

static struct sk_buff *
skb_recycle_fast(struct sk_buff *skb) 
{
  // if already at 1, that means we are the only user,
  // so no need to go through all the linux crap
  if (atomic_read(&skb->users) == 1 && !skb->cloned) {
    skb->data = skb->head;
    skb->tail = skb->data;
    skb->len = 0;
    skb_recycled_init_fast(skb);
    return skb;
  } else
    return skb_recycle(skb);
}


#define ATOMIC_INC(x) asm volatile("lock\n" "\tincl %0\n" : "=m"(x) : "m"(x))
#define ATOMIC_DEC(x) asm volatile("lock\n" "\tdecl %0\n" : "=m"(x) : "m"(x));
static inline int
find_consumer(int cpu, int bucket)
{
  int max_skbs = 0;
  int max_pool = -1;
  int i;

  if (pools[cpu].last_producer >= 0)
    ATOMIC_DEC(pools[pools[cpu].last_producer].consumers);
  for (i=0; i<smp_num_cpus; i++) {
    int s = SKBQ_SIZE(i, bucket);
    int c = pools[i].consumers + 1;
    if (c < 1) c = 1;
    s = s/c;
    if (s > max_skbs) {
      max_skbs = s;
      max_pool = i;
    }
  }
  pools[cpu].last_producer = max_pool;
  if (pools[cpu].last_producer >= 0) 
    ATOMIC_INC(pools[pools[cpu].last_producer].consumers);
  return pools[cpu].last_producer;
}

struct sk_buff*
skbmgr_allocate_skbs(unsigned size, int *want)
{
  struct sk_buff* head = 0;
  struct sk_buff* tail = 0;
  struct sk_buff* skb = 0; 
  int got = 0;
  int cpu = current->processor;
  int bucket;
  int producer = cpu;
  
  size += (SKBMGR_DEF_HEADSZ+SKBMGR_DEF_TAILSZ);
  bucket = get_size_outof_bucket(size);

  if (SKBQ_SIZE(producer, bucket) < *want && smp_num_cpus > 1) {
    if (pools[cpu].last_producer < 0 ||
	SKBQ_SIZE(pools[cpu].last_producer, bucket) < *want)
      find_consumer(cpu, bucket);
    if (pools[cpu].last_producer >= 0) 
      producer = pools[cpu].last_producer;
  }

  while (test_and_set_bit (0, (void*)&(pools[producer].buckets[bucket].lock))) 
    while(pools[producer].buckets[bucket].lock);

  while (got < *want) {
    unsigned h = pools[producer].buckets[bucket].head; 
    if (h != pools[producer].buckets[bucket].tail) {
      skb = pools[producer].buckets[bucket].skbs[h];
      pools[producer].buckets[bucket].skbs[h] = 0;
      pools[producer].buckets[bucket].head = SKBQ_NEXT(h);

      if (!skb) {
        printk("%d using %d: got a null skb!!! head %d, tail %d\n",
	       cpu, producer, pools[producer].buckets[bucket].head,
	       pools[producer].buckets[bucket].tail);
        break;
      }
#if DEBUG_SKBMGR
      pools[cpu].recycle_allocated++;
#endif
      skb_reserve(skb, SKBMGR_DEF_HEADSZ);
      APPEND_TO_LIST(head, tail, skb);
      got++;
    } else
      break;
  }

  clear_bit(0, (void*)&(pools[producer].buckets[bucket].lock));

  // fill in the remaining
  while (got < *want) { 
    int realsize = get_allocation_size(size);
    skb = alloc_skb(realsize, GFP_ATOMIC);
#if DEBUG_SKBMGR
    pools[cpu].allocated++;
#endif
    if (!skb) { 
      printk("oops, kernel could not allocate memory for skbuff\n"); 
      break;
    }
    skb_reserve(skb, SKBMGR_DEF_HEADSZ);
    APPEND_TO_LIST(head, tail, skb);
    got++;
  }

  *want = got;
  return head;
}

void
skbmgr_recycle_skbs(struct sk_buff *skbs, int dirty)
{
  int cpu = current->processor;
  int bucket;
 
  while (skbs) {
    struct sk_buff *skb = skbs;
    skbs = skbs->next;
    bucket = put_size_into_bucket(skb->truesize);
    if (bucket >= 0) {
      int tail = pools[cpu].buckets[bucket].tail; 
      int next = SKBQ_NEXT(tail);
      if (pools[cpu].buckets[bucket].skbs[tail] == 0 &&
	  next != pools[cpu].buckets[bucket].head) {
	if (!dirty || skb_recycle_fast(skb)) { 
	  pools[cpu].buckets[bucket].skbs[tail] = skb;
	  pools[cpu].buckets[bucket].tail = next; 
	}
        skb = 0; 
#if DEBUG_SKBMGR
        pools[cpu].recycle_freed++;
#endif
      }
    }
    if (skb != 0) {
#if DEBUG_SKBMGR
      pools[cpu].freed++;
#endif
      kfree_skb(skb);
    }
  }
}
