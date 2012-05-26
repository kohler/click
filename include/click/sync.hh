// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SYNC_HH
#define CLICK_SYNC_HH
#include <click/machine.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#if CLICK_LINUXMODULE || (CLICK_USERLEVEL && HAVE_MULTITHREAD)
# define CLICK_MULTITHREAD_SPINLOCK 1
#endif
#if CLICK_USERLEVEL && !NDEBUG
# define SPINLOCK_ASSERTLEVEL "<-999>"
#else
# define SPINLOCK_ASSERTLEVEL "<1>"
#endif
CLICK_DECLS

/** @file <click/sync.hh>
 * @brief Classes for synchronizing among multiple CPUs, particularly in the
 * Linux kernel.
 */

/** @class Spinlock
 * @brief A recursive spinlock for SMP Click threads.
 *
 * The Spinlock class abstracts a recursive spinlock, or polling mutex, in SMP
 * Click.  This is a type of mutual-exclusion lock in which acquiring the lock
 * is a polling operation (basically a "while (lock.acquired()) do nothing;"
 * loop).  Spinlocks can be used to synchronize access to shared data among
 * multiple Click SMP threads.  Spinlocks should not be held for long periods
 * of time: use them for quick updates and such.
 *
 * Spinlock operations do nothing unless Click was compiled with SMP support
 * (with --enable-multithread).  Therefore, Spinlock should not be used to,
 * for example, synchronize handlers with main element threads.  See also
 * SpinlockIRQ.
 *
 * The main Spinlock operations are acquire(), which acquires the lock, and
 * release(), which releases the lock.  attempt() acquires the lock only if it
 * can be acquired instantaneously.
 *
 * It is OK for a thread to acquire a lock it has already acquired, but you
 * must release it as many times as you have acquired it.
 *
 * @sa SimpleSpinlock, SpinlockIRQ
 */
class Spinlock { public:

    inline Spinlock();
    inline ~Spinlock();

    inline void acquire();
    inline void release();
    inline bool attempt();
    inline bool nested() const;

#if CLICK_MULTITHREAD_SPINLOCK
  private:

    atomic_uint32_t _lock;
    int32_t _depth;
    click_processor_t _owner;
#endif

};

/** @brief Create a Spinlock. */
inline
Spinlock::Spinlock()
#if CLICK_MULTITHREAD_SPINLOCK
    : _depth(0), _owner(click_invalid_processor())
#endif
{
#if CLICK_MULTITHREAD_SPINLOCK
    _lock = 0;
#endif
}

inline
Spinlock::~Spinlock()
{
#if CLICK_MULTITHREAD_SPINLOCK
    if (_depth != 0)
	click_chatter(SPINLOCK_ASSERTLEVEL "Spinlock::~Spinlock(): assertion \"_depth == 0\" failed");
#endif
}

/** @brief Acquires the Spinlock.
 *
 * On return, this thread has acquired the lock.  The function will spin
 * indefinitely until the lock is acquired.  It is OK to acquire a lock you
 * have already acquired, but you must release it as many times as you have
 * acquired it.
 */
inline void
Spinlock::acquire()
{
#if CLICK_MULTITHREAD_SPINLOCK
    click_processor_t my_cpu = click_get_processor();
    if (_owner != my_cpu) {
	while (_lock.swap(1) != 0)
	    do {
		click_relax_fence();
	    } while (_lock != 0);
	_owner = my_cpu;
    }
    _depth++;
#endif
}

/** @brief Attempts to acquire the Spinlock.
 * @return True iff the Spinlock was acquired.
 *
 * This function will acquire the lock and return true only if the Spinlock
 * can be acquired right away, without retries.
 */
inline bool
Spinlock::attempt()
{
#if CLICK_MULTITHREAD_SPINLOCK
    click_processor_t my_cpu = click_get_processor();
    if (_owner != my_cpu) {
	if (_lock.swap(1) != 0) {
	    click_put_processor();
	    return false;
	}
	_owner = my_cpu;
    }
    _depth++;
    return true;
#else
    return true;
#endif
}

/** @brief Releases the Spinlock.
 *
 * The Spinlock must have been previously acquired by either Spinlock::acquire
 * or Spinlock::attempt.
 */
inline void
Spinlock::release()
{
#if CLICK_MULTITHREAD_SPINLOCK
    if (unlikely(_owner != click_current_processor()))
	click_chatter(SPINLOCK_ASSERTLEVEL "Spinlock::release(): assertion \"owner == click_current_processor()\" failed");
    if (likely(_depth > 0)) {
	if (--_depth == 0) {
	    _owner = click_invalid_processor();
	    _lock = 0;
	}
    } else
	click_chatter(SPINLOCK_ASSERTLEVEL "Spinlock::release(): assertion \"_depth > 0\" failed");
    click_put_processor();
#endif
}

/** @brief Returns true iff the Spinlock has been acquired more than once by
 * the current thread.
 */
inline bool
Spinlock::nested() const
{
#if CLICK_MULTITHREAD_SPINLOCK
    return _depth > 1;
#else
    return false;
#endif
}


/** @class SimpleSpinlock
 * @brief A non-recursive spinlock for SMP Click threads.
 *
 * The Spinlock class abstracts a non-recursive spinlock, or polling mutex, in
 * SMP Click.  This is a type of mutual-exclusion lock in which acquiring the
 * lock is a polling operation (basically a "while (lock.acquired()) do
 * nothing;" loop).  Spinlocks can be used to synchronize access to shared
 * data among multiple Click SMP threads.  Spinlocks should not be held for
 * long periods of time: use them for quick updates and such.
 *
 * Spinlock operations do nothing unless Click was compiled with SMP support
 * (with --enable-multithread).  Therefore, Spinlock should not be used to,
 * for example, synchronize handlers with main element threads.  See also
 * SpinlockIRQ.
 *
 * The main Spinlock operations are acquire(), which acquires the lock, and
 * release(), which releases the lock.  attempt() acquires the lock only if it
 * can be acquired instantaneously.
 *
 * It is NOT OK for a thread to acquire a lock it has already acquired.
 *
 * @sa Spinlock, SpinlockIRQ
 */
class SimpleSpinlock { public:

    inline SimpleSpinlock();
    inline ~SimpleSpinlock();

    inline void acquire();
    inline void release();
    inline bool attempt();

#if CLICK_LINUXMODULE
  private:
    struct mutex _lock;
#elif CLICK_MULTITHREAD_SPINLOCK
  private:
    atomic_uint32_t _lock;
#endif

};

/** @brief Create a SimpleSpinlock. */
inline
SimpleSpinlock::SimpleSpinlock()
{
#if CLICK_LINUXMODULE
    mutex_init(&_lock);
#elif CLICK_MULTITHREAD_SPINLOCK
    _lock = 0;
#endif
}

inline
SimpleSpinlock::~SimpleSpinlock()
{
}

/** @brief Acquires the SimpleSpinlock.
 *
 * On return, this thread has acquired the lock.  The function will spin
 * indefinitely until the lock is acquired.
 */
inline void
SimpleSpinlock::acquire()
{
#if CLICK_LINUXMODULE
    mutex_lock(&_lock);
#elif CLICK_MULTITHREAD_SPINLOCK
    while (_lock.swap(1) != 0)
	do {
	    click_relax_fence();
	} while (_lock != 0);
#endif
}

/** @brief Attempts to acquire the SimpleSpinlock.
 * @return True iff the SimpleSpinlock was acquired.
 *
 * This function will acquire the lock and return true only if the
 * SimpleSpinlock can be acquired right away, without retries.
 */
inline bool
SimpleSpinlock::attempt()
{
#if CLICK_LINUXMODULE
    return mutex_trylock(&_lock);
#elif CLICK_MULTITHREAD_SPINLOCK
    return _lock.swap(1) == 0;
#else
    return true;
#endif
}

/** @brief Releases the SimpleSpinlock.
 *
 * The SimpleSpinlock must have been previously acquired by either
 * SimpleSpinlock::acquire or SimpleSpinlock::attempt.
 */
inline void
SimpleSpinlock::release()
{
#if CLICK_LINUXMODULE
    mutex_unlock(&_lock);
#elif CLICK_MULTITHREAD_SPINLOCK
    _lock = 0;
#endif
}


/** @class SpinlockIRQ
 * @brief A spinlock that disables interrupts.
 *
 * The SpinlockIRQ class abstracts a spinlock, or polling mutex, that also
 * turns off interrupts.  Spinlocks are a type of mutual-exclusion lock in
 * which acquiring the lock is a polling operation (basically a "while
 * (lock.acquired()) do nothing;" loop).  The SpinlockIRQ variant can be used
 * to protect Click data structures from interrupts and from other threads.
 * Very few objects in Click need this protection; the Click Master object,
 * which protects the task list, uses it, but that's hidden from users.
 * Spinlocks should not be held for long periods of time: use them for quick
 * updates and such.
 *
 * In the Linux kernel, SpinlockIRQ is equivalent to a combination of
 * local_irq_save and the spinlock_t type.
 *
 * The SpinlockIRQ operations are acquire(), which acquires the lock, and
 * release(), which releases the lock.
 *
 * It is NOT OK for a SpinlockIRQ thread to acquire a lock it has already
 * acquired.
 */
class SpinlockIRQ { public:

    inline SpinlockIRQ();

#if CLICK_LINUXMODULE
    typedef unsigned long flags_t;
#else
    typedef int flags_t;
#endif

    inline flags_t acquire();
    inline void release(flags_t);

#if CLICK_LINUXMODULE
  private:
    spinlock_t _lock;
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
  private:
    Spinlock _lock;
#endif

};

/** @brief Creates a SpinlockIRQ. */
inline
SpinlockIRQ::SpinlockIRQ()
{
#if CLICK_LINUXMODULE
    spin_lock_init(&_lock);
#endif
}

/** @brief Acquires the SpinlockIRQ.
 * @return The current state of the interrupt flags.
 */
inline SpinlockIRQ::flags_t
SpinlockIRQ::acquire()
{
#if CLICK_LINUXMODULE
    flags_t flags;
    spin_lock_irqsave(&_lock, flags);
    return flags;
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
    _lock.acquire();
    return 0;
#else
    return 0;
#endif
}

/** @brief Releases the SpinlockIRQ.
 * @param flags The value returned by SpinlockIRQ::acquire().
 */
inline void
SpinlockIRQ::release(flags_t flags)
{
#if CLICK_LINUXMODULE
    spin_unlock_irqrestore(&_lock, flags);
#elif CLICK_USERLEVEL && HAVE_MULTITHREAD
    (void) flags;
    _lock.release();
#else
    (void) flags;
#endif
}


// read-write lock
//
// on read: acquire local read lock
// on write: acquire every read lock
//
// alternatively, we could use a read counter and a write lock. we don't do
// that because we'd like to avoid a cache miss for read acquires. this makes
// reads very fast, and writes more expensive

/** @class ReadWriteLock
 * @brief A read/write lock.
 *
 * The ReadWriteLock class abstracts a read/write lock in SMP Click.  Multiple
 * SMP Click threads can hold read locks simultaneously, but if any thread
 * holds a write lock, then no other thread holds any kind of lock.  The
 * read/write lock is implemented with Spinlock objects, so acquiring a lock
 * is a polling operation.  ReadWriteLocks can be used to synchronize access
 * to shared data among multiple Click SMP threads.  ReadWriteLocks should not
 * be held for long periods of time.
 *
 * ReadWriteLock operations do nothing unless Click was compiled with
 * --enable-multithread.  Therefore, ReadWriteLock should not be used to, for
 * example, synchronize handlers with main element threads.
 *
 * The main ReadWriteLock operations are acquire_read() and acquire_write(),
 * which acquire the lock for reading or writing, respectively, and
 * release_read() and release_write(), which similarly release the lock.
 * attempt_read() and attempt_write() acquire the lock only if it can be
 * acquired instantaneously.
 *
 * It is OK for a thread to acquire a lock it has already acquired, but you
 * must release it as many times as you have acquired it.
 *
 * ReadWriteLock objects are relatively large in terms of memory usage; don't
 * create too many of them.
 */
class ReadWriteLock { public:

    inline ReadWriteLock();
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    inline ~ReadWriteLock();
#endif

    inline void acquire_read();
    inline bool attempt_read();
    inline void release_read();
    inline void acquire_write();
    inline bool attempt_write();
    inline void release_write();

#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
  private:
    // allocate 32 bytes (size of a cache line) for every member
    struct lock_t {
	Spinlock _lock;
	unsigned char reserved[32 - sizeof(Spinlock)];
    } *_l;
#endif

};

/** @brief Creates a ReadWriteLock. */
inline
ReadWriteLock::ReadWriteLock()
{
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    _l = new lock_t[num_possible_cpus()];
#endif
}

#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
inline
ReadWriteLock::~ReadWriteLock()
{
    delete[] _l;
}
#endif

/** @brief Acquires the ReadWriteLock for reading.
 *
 * On return, this thread has acquired the lock for reading.  The function
 * will spin indefinitely until the lock is acquired.  It is OK to acquire a
 * lock you have already acquired, but you must release it as many times as
 * you have acquired it.
 *
 * @sa Spinlock::acquire
 */
inline void
ReadWriteLock::acquire_read()
{
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    click_processor_t my_cpu = click_get_processor();
    _l[my_cpu]._lock.acquire();
#endif
}

/** @brief Attempts to acquire the ReadWriteLock for reading.
 * @return True iff the ReadWriteLock was acquired.
 *
 * This function will acquire the lock for reading and return true only if the
 * ReadWriteLock can be acquired right away, without retries.
 */
inline bool
ReadWriteLock::attempt_read()
{
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    click_processor_t my_cpu = click_get_processor();
    bool result = _l[my_cpu]._lock.attempt();
    if (!result)
	click_put_processor();
    return result;
#else
    return true;
#endif
}

/** @brief Releases the ReadWriteLock for reading.
 *
 * The ReadWriteLock must have been previously acquired by either
 * ReadWriteLock::acquire_read or ReadWriteLock::attempt_read.  Do not call
 * release_read() on a lock that was acquired for writing.
 */
inline void
ReadWriteLock::release_read()
{
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    _l[click_current_processor()]._lock.release();
    click_put_processor();
#endif
}

/** @brief Acquires the ReadWriteLock for writing.
 *
 * On return, this thread has acquired the lock for writing.  The function
 * will spin indefinitely until the lock is acquired.  It is OK to acquire a
 * lock you have already acquired, but you must release it as many times as
 * you have acquired it.
 *
 * @sa ReadWriteLock::acquire_read
 */
inline void
ReadWriteLock::acquire_write()
{
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    for (unsigned i = 0; i < num_possible_cpus(); i++)
	_l[i]._lock.acquire();
#endif
}

/** @brief Attempts to acquire the ReadWriteLock for writing.
 * @return True iff the ReadWriteLock was acquired.
 *
 * This function will acquire the lock for writing and return true only if the
 * ReadWriteLock can be acquired right away, without retries.  Note, however,
 * that acquiring a ReadWriteLock requires as many operations as there are
 * CPUs.
 *
 * @sa ReadWriteLock::attempt_read
 */
inline bool
ReadWriteLock::attempt_write()
{
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    bool all = true;
    unsigned i;
    for (i = 0; i < num_possible_cpus(); i++)
	if (!(_l[i]._lock.attempt())) {
	    all = false;
	    break;
	}
    if (!all)
	for (unsigned j = 0; j < i; j++)
	    _l[j]._lock.release();
    return all;
#else
    return true;
#endif
}

/** @brief Releases the ReadWriteLock for writing.
 *
 * The ReadWriteLock must have been previously acquired by either
 * ReadWriteLock::acquire_write or ReadWriteLock::attempt_write.  Do not call
 * release_write() on a lock that was acquired for reading.
 *
 * @sa ReadWriteLock::release_read
 */
inline void
ReadWriteLock::release_write()
{
#if CLICK_LINUXMODULE && defined(CONFIG_SMP)
    for (unsigned i = 0; i < num_possible_cpus(); i++)
	_l[i]._lock.release();
#endif
}

CLICK_ENDDECLS
#undef SPINLOCK_ASSERTLEVEL
#endif
