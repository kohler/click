#ifndef CLICK_SYNC_HH
#define CLICK_SYNC_HH
#include <click/glue.hh>
#include <click/atomic.hh>
#if defined(__KERNEL__) && defined(__SMP__)
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#  include <linux/tasks.h>
# else
#  include <linux/threads.h>
# endif
# include <linux/sched.h>
# define my_cpu current->processor
#endif

// loop-in-cache spinlock implementation: 12 bytes. if the size of this class
// changes, change size of padding in ReadWriteLock below.

#if defined(__KERNEL__) && defined(__SMP__)

class Spinlock { public:

  Spinlock();
  ~Spinlock();
  
  void acquire();
  void release();
  bool attempt();
  void ref();
  void unref();

 private:

  volatile unsigned short _lock;
  unsigned short _depth;
  int _owner;
  int _refcnt;
  
};

inline
Spinlock::Spinlock()
  : _lock(0), _depth(0), _owner(-1), _refcnt(0)
{
#ifndef __i386__
# error "no multithread support for non i386 click"
#endif
} 

inline
Spinlock::~Spinlock()
{
  if (_lock != 0) 
    click_chatter("warning: freeing unreleased lock");
}

inline void
Spinlock::ref()
{
  _refcnt++;
}

inline void
Spinlock::unref()
{
  if (_refcnt > 0) _refcnt--;
  if (_refcnt == 0) delete this;
}

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

#else /* defined(__KERNEL__) && defined(__SMP__) */

class Spinlock { public:

  Spinlock()			{ }
  ~Spinlock()			{ }

  void acquire()		{ }
  void release()		{ }
  bool attempt()		{ return true; }
  void ref()			{ }
  void unref()			{ }
  
};

#endif /* defined(__KERNEL__) && defined(__SMP__) */


// read-write lock:
//
// on read: acquire local read lock
// on write: acquire every read lock
//
// alternatively, we could use a read counter and a write lock. we don't do
// that because we'd like to avoid a cache miss for read acquires. this makes
// reads very fast, and writes more expensive

#if defined(__KERNEL__) && defined(__SMP__)

class ReadWriteLock {

  // allocate 32 bytes (size of a cache line) for every member
  struct lock_t {
    Spinlock _lock;
    unsigned char reserved[20];
  };

  lock_t *_l;

public:
  
  ReadWriteLock();
  ~ReadWriteLock();
  
  void acquire_read();
  bool attempt_read();
  void release_read();
  void acquire_write();
  bool attempt_write();
  void release_write();
};

inline
ReadWriteLock::ReadWriteLock()
{
  _l = new lock_t[smp_num_cpus];
}

inline
ReadWriteLock::~ReadWriteLock()
{
  delete[] _l;
}

inline void
ReadWriteLock::acquire_read()
{
  assert(my_cpu >= 0);
  _l[my_cpu]._lock.acquire();
}

inline bool
ReadWriteLock::attempt_read()
{
  assert(my_cpu >= 0);
  return _l[my_cpu]._lock.attempt();
}

inline void
ReadWriteLock::release_read()
{
  assert(my_cpu >= 0);
  _l[my_cpu]._lock.release();
}

inline void
ReadWriteLock::acquire_write()
{
  for(unsigned i=0; i<smp_num_cpus; i++)
    _l[i]._lock.acquire();
}

inline bool
ReadWriteLock::attempt_write()
{
  bool all = true;
  unsigned i;
  for(i=0; i<smp_num_cpus; i++) {
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
  for(unsigned i=0; i<smp_num_cpus; i++)
    _l[i]._lock.release();
}

#else /* defined(__KERNEL__) && defined(__SMP__) */

class ReadWriteLock { public:
  
  ReadWriteLock()				{ }
  
  void acquire_read()				{ }
  bool attempt_read()				{ return true; }
  void release_read()				{ }
  void acquire_write()				{ }
  bool attempt_write()				{ return true; }
  void release_write()				{ }

};

#endif /* defined(__KERNEL__) && defined(__SMP__) */

#endif


