/*
 * skbmgr.cc -- Linux kernel module sk_buff manager
 * Benjie Chen, Eddie Kohler
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/skbmgr.hh>
#include <assert.h>
extern "C" {
#define new xxx_new
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <net/dst.h>
#include <linux/if_packet.h>
#undef new
}

#define DEBUG_SKBMGR 1

class RecycledSkbBucket { public:
  static const int SIZE = 62;

  void initialize();
  void cleanup();

  bool empty() const		{ return _head == _tail; }
  unsigned size() const;
  int enq(struct sk_buff *);	// returns -1 if not enqueued
  struct sk_buff *deq();

 private:
  
  int _head;
  int _tail;
  struct sk_buff *_skbs[SIZE];

  static int next_i(int i)	{ return (i == SIZE - 1 ? 0 : i + 1); }
  friend class RecycledSkbPool;
};


class RecycledSkbPool { public:
  static const int NBUCKETS = 2;

  void initialize();
  void cleanup();

  RecycledSkbBucket &bucket(int);
  static int size_to_lower_bucket(unsigned);
  static int size_to_higher_bucket(unsigned);
  static unsigned size_to_higher_bucket_size(unsigned);

 private:

  RecycledSkbBucket _buckets[NBUCKETS];
  atomic_t _lock;
#ifdef __MTCLICK__
  int _last_producer;
  u_atomic32_t _consumers;
#else
  int _pad2[2];
#endif
#if DEBUG_SKBMGR
  int _allocated;
  int _freed;
  int _recycle_freed;
  int _recycle_allocated;
  int _pad[1];
#else
  int _pad[5];
#endif
  
  void lock();
  void unlock();
  struct sk_buff *allocate(unsigned, int, int *);
  void recycle(struct sk_buff *, bool);

#ifdef __MTCLICK__ 
  static int find_consumer(int, int);
#endif
  
  friend  struct sk_buff * 
    skbmgr_allocate_skbs(unsigned size, int *want);
  friend void 
    skbmgr_recycle_skbs(struct sk_buff *skbs, int dirty);
};


void
RecycledSkbBucket::initialize()
{
  _head = _tail = 0;
  memset(_skbs, 0, sizeof(_skbs));
}

void
RecycledSkbBucket::cleanup()
{
  for (int i = _head; i != _tail; i = next_i(i))
    kfree_skb(_skbs[i]);
  _head = _tail = 0;
}

inline unsigned
RecycledSkbBucket::size() const
{
  return (_head < _tail ? _tail - _head : _tail + SIZE - _head);
}

inline int
RecycledSkbBucket::enq(struct sk_buff *skb)
{
  int n = next_i(_tail);
  if (n == _head)
    return -1;
  _skbs[_tail] = skb;
  _tail = n;
  return 0;
}

inline struct sk_buff *
RecycledSkbBucket::deq()
{
  if (_head == _tail)
    return 0;
  else {
    struct sk_buff *skb = _skbs[_head];
    _head = next_i(_head);
    return skb;
  }
}

inline void
RecycledSkbPool::lock()
{
  while (test_and_set_bit(0, (void *)&_lock)) {
    while (atomic_read(&_lock))
      /* nothing */;
  }
}

inline void
RecycledSkbPool::unlock()
{
  clear_bit(0, (void *)&_lock);
}


void
RecycledSkbPool::initialize()
{
  atomic_set(&_lock, 0);
  for (int i = 0; i < NBUCKETS; i++)
    _buckets[i].initialize();
#ifdef __MTCLICK__
  _last_producer = -1;
  _consumers = 0;
#endif
#if DEBUG_SKBMGR
  _recycle_freed = 0;
  _freed = 0;
  _recycle_allocated = 0;
  _allocated = 0;
#endif
}

void
RecycledSkbPool::cleanup()
{
  lock();
  for (int i = 0; i < NBUCKETS; i++)
    _buckets[i].cleanup();
#ifdef __MTCLICK__
  _last_producer = -1;
  _consumers = 0;
#endif
#if DEBUG_SKBMGR
  if (_freed > 0 || _allocated > 0)
    printk ("poll %p: %d/%d freed, %d/%d allocated\n", this,
	    _freed, _recycle_freed, _allocated, _recycle_allocated);
#endif
  unlock();
}

inline RecycledSkbBucket &
RecycledSkbPool::bucket(int i)
{
  assert((unsigned)i < NBUCKETS);
  return _buckets[i];
}

inline int
RecycledSkbPool::size_to_lower_bucket(unsigned size)
{
  if (size >= 1800) return 1;
  if (size >= 500) return 0;
  return -1;
}

inline int
RecycledSkbPool::size_to_higher_bucket(unsigned size)
{
  if (size <= 500) return 0;
  if (size <= 1800) return 1;
  return -1;
}

inline unsigned
RecycledSkbPool::size_to_higher_bucket_size(unsigned size)
{
  if (size <= 500) return 500;
  if (size <= 1800) return 1800;
  return size;
}


#ifdef __MTCLICK__
static RecycledSkbPool pool[NR_CPUS];
#else
static RecycledSkbPool pool;
#endif

#define SKBMGR_DEF_TAILSZ 64
#define SKBMGR_DEF_HEADSZ 64


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


#ifdef __MTCLICK__

static inline int
RecycledSkbPool::find_consumer(int cpu, int bucket)
{
  int max_skbs = 0;
  int max_pool = -1;
  int i;

  if (pool[cpu]._last_producer >= 0)
    pool[pool[cpu]._last_producer]._consumers--;

  for (i=0; i<smp_num_cpus; i++) {
    int s = pool[i].bucket(bucket).size();
    int c = pool[i]._consumers + 1;
    if (c < 1) c = 1;
    s = s/c;
    if (s > max_skbs) {
      max_skbs = s;
      max_pool = i;
    }
  }
  pool[cpu]._last_producer = max_pool;
  if (pool[cpu]._last_producer >= 0)
    pool[pool[cpu]._last_producer]._consumers++;
  return pool[cpu]._last_producer;
}

#endif


void
RecycledSkbPool::recycle(struct sk_buff *skbs, bool dirty)
{
  while (skbs) {
    struct sk_buff *skb = skbs;
    skbs = skbs->next;
    // where should sk_buff go?
    int bucket = size_to_lower_bucket(skb->truesize);
    // try to put in that bucket
    if (bucket >= 0) {
      int tail = _buckets[bucket]._tail;
      int next = _buckets[bucket].next_i(tail);
      if (next != _buckets[bucket]._head) {
	// Note: skb_recycle_fast will free the skb if it cannot recycle it
	if (!dirty || (skb = skb_recycle_fast(skb))) {
	  _buckets[bucket]._skbs[tail] = skb;
	  _buckets[bucket]._tail = next;
	}
	skb = 0;
#if DEBUG_SKBMGR
        _recycle_freed++;
#endif
      }
    }
    // if not taken care of, then free it
    if (skb) {
#if DEBUG_SKBMGR
      _freed++;
#endif
      kfree_skb(skb);
    }
  }
}

struct sk_buff *
RecycledSkbPool::allocate(unsigned size, int want, int *store_got)
{
  size += SKBMGR_DEF_HEADSZ + SKBMGR_DEF_TAILSZ;
  int bucket = size_to_higher_bucket(size);

  struct sk_buff *head;
  struct sk_buff **prev = &head;
  int got = 0;

  if (bucket >= 0) {
    lock();
    RecycledSkbBucket &buck = _buckets[bucket];
    while (got < want && !buck.empty()) {
      struct sk_buff *skb = _buckets[bucket].deq();
#if DEBUG_SKBMGR
      _recycle_allocated++;
#endif
      skb_reserve(skb, SKBMGR_DEF_HEADSZ);
      *prev = skb;
      prev = &skb->next;
      got++;
    }
    unlock();
  }

  size = size_to_higher_bucket_size(size);
  while (got < want) {
    struct sk_buff *skb = alloc_skb(size, GFP_ATOMIC);
#if DEBUG_SKBMGR
    _allocated++;
#endif
    if (!skb) {
      printk("<1>oops, kernel could not allocate memory for skbuff\n"); 
      break;
    }
    skb_reserve(skb, SKBMGR_DEF_HEADSZ);
    *prev = skb;
    prev = &skb->next;
    got++;
  }

  *prev = 0;
  *store_got = got;
  return head;
}

extern "C" {

void
skbmgr_init()
{
#ifdef __MTCLICK__
  for(int i=0; i<NR_CPUS; i++) pool[i].initialize();
#else
  pool.initialize();
#endif
}

void
skbmgr_cleanup(void)
{
#ifdef __MTCLICK__
  for(int i=0; i<NR_CPUS; i++) pool[i].cleanup();
#else
  pool.cleanup();
#endif
}

struct sk_buff *
skbmgr_allocate_skbs(unsigned size, int *want)
{
#ifdef __MTCLICK__
  int cpu = current->processor;
  int producer = cpu;

  size += (SKBMGR_DEF_HEADSZ+SKBMGR_DEF_TAILSZ);
  int bucket = size_to_higher_bucket(size);

  if (pool[producer].bucket(bucket).size() < *want && smp_num_cpus > 1) {
    if (pool[cpu]._last_producer < 0 ||
	pool[pool[cpu]._last_producer].bucket(bucket).size() < *want)
      find_consumer(cpu, bucket);
    if (pool[cpu]._last_producer >= 0)
      producer = pool[cpu]._last_producer;
  }
  return pool[producer].allocate(size, *want, want);
#else
  return pool.allocate(size, *want, want);
#endif
}

void
skbmgr_recycle_skbs(struct sk_buff *skbs, int dirty)
{
#ifdef __MTCLICK__
  int cpu = current->processor;
  pool[cpu].recycle(skbs, dirty);
#else
  pool.recycle(skbs, dirty);
#endif
}

}

