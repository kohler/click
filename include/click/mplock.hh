#ifndef MPLOCK_HH
#define MPLOCK_HH

#include <assert.h>

#ifdef __KERNEL__
#define my_thread_id current->pid
#else
#include <unistd.h>
#define my_thread_id getpid()
#endif

class Spinlock
{
private:
  volatile unsigned short _lock;
  unsigned short _depth;
  pid_t _owner;

public:
  Spinlock() : _lock(0), _depth(0), _owner(-1) {} 
  ~Spinlock();

  void acquire();
  void release();
  bool attempt();
};
  
inline
Spinlock::~Spinlock()
{
  if (_lock != 0) 
    click_chatter("warning: freeing unreleased lock");
}

#ifndef __SMP__

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

#else

inline void
Spinlock::acquire()
{
  if (_owner == my_thread_id) {
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

  _owner = my_thread_id;
  _depth++;
}

inline bool
Spinlock::attempt()
{
  if (_owner == my_thread_id) {
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
    _owner = my_thread_id;
    _depth++;
    return true;
  }
}

inline void
Spinlock::release()
{
  if (_owner != my_thread_id)
    click_chatter("releasing someone else's lock");
  _depth--;
  if (_depth == 0) {
    _owner = -1;
    _lock = 0;
  }
}

#endif // __SMP__

#endif 
