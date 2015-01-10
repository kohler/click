// -*- c-basic-offset: 2; related-file-name: "../include/click/skbmgr.hh" -*-
/*
 * skbmgr.cc -- Linux kernel module sk_buff manager
 * Benjie Chen, Eddie Kohler
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/sync.hh>
#include <click/skbmgr.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <asm/bitops.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <net/dst.h>
#include <linux/if_packet.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

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
  unsigned long _lock;
#if __MTCLICK__
  int _last_producer;
  atomic_uint32_t _consumers;
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

  inline void lock();
  inline void unlock();

  struct sk_buff *allocate(unsigned headroom, unsigned size, int, int *);
  void recycle(struct sk_buff *);

#if __MTCLICK__
  static int find_producer(int, int);
#endif

  friend struct sk_buff *skbmgr_allocate_skbs(unsigned, unsigned, int *);
  friend void skbmgr_recycle_skbs(struct sk_buff *);

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
  return (_head <= _tail ? _tail - _head : _tail + SIZE - _head);
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
  while (test_and_set_bit(0, &_lock)) {
    while (_lock)
      click_relax_fence();
  }
}

inline void
RecycledSkbPool::unlock()
{
  clear_bit(0, &_lock);
}


void
RecycledSkbPool::initialize()
{
  _lock = 0;
  for (int i = 0; i < NBUCKETS; i++)
    _buckets[i].initialize();
#if __MTCLICK__
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
#if __MTCLICK__
  _last_producer = -1;
  _consumers = 0;
#endif
#if DEBUG_SKBMGR
  if (_freed > 0 || _allocated > 0)
    printk("poll %p: %d/%d freed, %d/%d allocated\n", this,
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
RecycledSkbPool::size_to_lower_bucket_size(unsigned size)
{
  if (size >= 1800) return 1800;
  if (size >= 500) return 500;
  return size;
}

inline unsigned
RecycledSkbPool::size_to_higher_bucket_size(unsigned size)
{
  if (size <= 500) return 500;
  if (size <= 1800) return 1800;
  return size;
}


#if __MTCLICK__
static RecycledSkbPool pool[NR_CPUS];
#else
static RecycledSkbPool pool;
#endif

#define SKBMGR_DEF_TAILSZ 64
#define SKBMGR_DEF_HEADSZ 64 // must be divisible by 4


#if __MTCLICK__

inline int
RecycledSkbPool::find_producer(int cpu, int bucket)
{
  int max_skbs = 0;
  int max_pool = -1;
  int i;

  if (pool[cpu]._last_producer >= 0)
    pool[pool[cpu]._last_producer]._consumers--;

  for (i = 0; i < num_possible_cpus(); i++) {
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
RecycledSkbPool::recycle(struct sk_buff *skbs)
{
  while (skbs) {
    struct sk_buff *skb = skbs;
    skbs = skbs->next;

#if HAVE_CLICK_SKB_RECYCLE || HAVE_SKB_RECYCLE_CHECK
    // where should sk_buff go?
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    unsigned char *skb_end = skb_end_pointer(skb);
# else
    unsigned char *skb_end = skb->end;
# endif
    int bucket = size_to_lower_bucket(skb_end - skb->head);

    // try to put in that bucket
    if (bucket >= 0) {
      lock();
      int tail = _buckets[bucket]._tail;
      int next = _buckets[bucket].next_i(tail);
      if (next != _buckets[bucket]._head) {
#if HAVE_CLICK_SKB_RECYCLE
	// Note: skb_recycle_fast will free the skb if it cannot recycle it
	if ((skb = skb_recycle(skb))) {
#elif HAVE_SKB_RECYCLE_CHECK
	if (skb_recycle_check(skb, size_to_lower_bucket_size(skb_end - skb->head))) {
#endif
	  _buckets[bucket]._skbs[tail] = skb;
	  _buckets[bucket]._tail = next;
	  skb = 0;
# if DEBUG_SKBMGR
          _recycle_freed++;
# endif
	}
      }
      unlock();
    }
#endif

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
RecycledSkbPool::allocate(unsigned headroom, unsigned size, int want, int *store_got)
{
  int bucket = size_to_higher_bucket(headroom + size);

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
      skb_reserve(skb, headroom);
      *prev = skb;
      prev = &skb->next;
      got++;
    }
    unlock();
  }

  size = size_to_higher_bucket_size(headroom + size);
  while (got < want) {
    struct sk_buff *skb = alloc_skb(size, GFP_ATOMIC);
#if DEBUG_SKBMGR
    _allocated++;
#endif
    if (!skb) {
      printk(KERN_ALERT "oops, kernel could not allocate memory for skbuff\n");
      break;
    }
    skb_reserve(skb, headroom);
    *prev = skb;
    prev = &skb->next;
    got++;
  }

  *prev = 0;
  *store_got = got;
  return head;
}

void
skbmgr_init()
{
#if __MTCLICK__
  for (int i = 0; i < NR_CPUS; i++)
    pool[i].initialize();
#else
  pool.initialize();
#endif
}

void
skbmgr_cleanup()
{
#if __MTCLICK__
  for (int i = 0; i < NR_CPUS; i++)
    pool[i].cleanup();
#else
  pool.cleanup();
#endif
}

struct sk_buff *
skbmgr_allocate_skbs(unsigned headroom, unsigned size, int *want)
{
  if (headroom == 0)
    headroom = SKBMGR_DEF_HEADSZ;
  size += SKBMGR_DEF_TAILSZ;

#if __MTCLICK__
  click_processor_t cpu = click_get_processor();
  int producer = cpu;
  int bucket = RecycledSkbPool::size_to_higher_bucket(headroom + size);

  int w = *want;
  if (pool[producer].bucket(bucket).size() < w) {
    if (pool[cpu]._last_producer < 0 ||
	pool[pool[cpu]._last_producer].bucket(bucket).size() < w)
      RecycledSkbPool::find_producer(cpu, bucket);
    if (pool[cpu]._last_producer >= 0)
      producer = pool[cpu]._last_producer;
  }
  sk_buff *skb = pool[producer].allocate(headroom, size, w, want);
  click_put_processor();
  return skb;
#else
  return pool.allocate(headroom, size, *want, want);
#endif
}

void
skbmgr_recycle_skbs(struct sk_buff *skbs)
{
#if __MTCLICK__
  click_processor_t cpu = click_get_processor();
  pool[cpu].recycle(skbs);
  click_put_processor();
#else
  pool.recycle(skbs);
#endif
}
