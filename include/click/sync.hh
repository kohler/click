#ifndef SYNC_HH
#define SYNC_HH

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <click/glue.hh>

#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)
#include <linux/tasks.h>
#include <linux/sched.h>
extern atomic_t num_click_threads;
#define my_cpu current->processor
#define click_nthreads (atomic_read(&num_click_threads))
#endif

#include <click/atomic.hh>

// loop-in-cache spinlock implementation: 12 bytes. if the size of this class
// changes, change size of padding in ReadWriteLock below.

class Spinlock
{
#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)
private:
  volatile unsigned short _lock;
  unsigned short _depth;
  int _owner;
  int _refcnt;
#endif

public:
#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)
  Spinlock() : _lock(0), _depth(0), _owner(-1), _refcnt(0) {
#ifndef __i386__
    StaticAssert("no multithread support for non i386 click");
#endif
  } 
#else
  Spinlock() {}
#endif
  ~Spinlock();

  void acquire();
  void release();
  bool attempt();
  void ref();
  void unref();
};
  
inline
Spinlock::~Spinlock()
{
#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)
  if (_lock != 0) 
    click_chatter("warning: freeing unreleased lock");
#endif
}

inline void
Spinlock::ref()
{
#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)
  _refcnt++;
#endif
}

inline void
Spinlock::unref()
{
#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)
  if (_refcnt > 0) _refcnt--;
  if (_refcnt == 0) delete this;
#endif
}

#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)

inline void
Spinlock::acquire()
{
  if (_owner == my_cpu) {
    _depth++;
    return;
  }
  
  register unsigned short content = 1;
test_n_set:
  asm volatile ("xchgw %0,%1" :
                "=r" (content),
                "=m" (_lock) :
                "0" (content),
                "m" (_lock));
  if (content != 0) {
    while(_lock != 0)
      asm volatile ("" : : : "memory");  
    goto test_n_set;
  }

  _owner = my_cpu;
  _depth++;
}

inline bool
Spinlock::attempt()
{
  if (_owner == my_cpu) {
    _depth++;
    return true;
  }
  
  register unsigned short content = 1;
  asm volatile ("xchgw %0,%1" :
                "=r" (content),
                "=m" (_lock) :
                "0" (content),
                "m" (_lock));
  if (content != 0) {
    return false;
  }
  else {
    _owner = my_cpu;
    _depth++;
    return true;
  }
}

inline void
Spinlock::release()
{
  if (_owner != my_cpu)
    click_chatter("releasing someone else's lock");
  if (_depth > 0) 
    _depth--;
  else
    click_chatter("lock already freed");
  if (_depth == 0) {
    _owner = -1;
    _lock = 0;
  }
}

#else

inline void
Spinlock::acquire()
{}

inline void
Spinlock::release()
{}

inline bool
Spinlock::attempt()
{
  return true;
}

#endif


// read-write lock:
//
// on read: acquire local read lock
// on write: acquire every read lock
//
// alternatively, we could use a read counter and a write lock. we don't do
// that because we'd like to avoid a cache miss for read acquires. this makes
// reads very fast, and writes more expensive

#if defined(__KERNEL__) && defined(__SMP__) && defined(__MTCLICK__)

class ReadWriteLock {

  // allocate 32 bytes (size of a cache line) for every member
  struct {
    Spinlock _lock;
    unsigned char reserved[20];
  } _l[NUM_CLICK_CPUS];

public:
  
  ReadWriteLock()				{ }
  
  void acquire_read();
  bool attempt_read();
  void release_read();
  void acquire_write();
  bool attempt_write();
  void release_write();
};

inline void
ReadWriteLock::acquire_read()
{
  if (click_nthreads <= 1) return;
  assert(my_cpu >= 0);
  _l[my_cpu]._lock.acquire();
}

inline bool
ReadWriteLock::attempt_read()
{
  if (click_nthreads <= 1) return true;
  assert(my_cpu >= 0);
  return _l[my_cpu]._lock.attempt();
}

inline void
ReadWriteLock::release_read()
{
  if (click_nthreads <= 1) return;
  assert(my_cpu >= 0);
  _l[my_cpu]._lock.release();
}

inline void
ReadWriteLock::acquire_write()
{
  if (click_nthreads <= 1) return;
  for(unsigned i=0; i<NUM_CLICK_CPUS; i++)
    _l[i]._lock.acquire();
}

inline bool
ReadWriteLock::attempt_write()
{
  bool all = true;
  if (click_nthreads <= 1) return all;
  unsigned i;
  for(i=0; i<NUM_CLICK_CPUS; i++) {
    if (!(_l[i]._lock.attempt())) {
      all = false;
      break;
    }
  }
  if (!all) {
    for(unsigned j=0; j<i; j++)
      _l[j]._lock.release();
  }
  return all;
}

inline void
ReadWriteLock::release_write()
{
  if (click_nthreads <= 1) return;
  for(unsigned i=0; i<NUM_CLICK_CPUS; i++)
    _l[i]._lock.release();
}

#else


class ReadWriteLock { public:
  
  ReadWriteLock()				{ }
  
  void acquire_read()				{ }
  bool attempt_read()				{ return true; }
  void release_read()				{ }
  void acquire_write()				{ }
  bool attempt_write()				{ return true; }
  void release_write()				{ }

};

#endif

#endif // SYNC_HH

