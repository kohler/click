#ifndef CLICK_SPINLOCKPUSH_HH
#define CLICK_SPINLOCKPUSH_HH
#include <click/element.hh>
#include <click/sync.hh>
CLICK_DECLS

/*
 * =c
 * SpinlockPush(LOCK)
 * =s threads
 * acquires a spinlock, push the packets then release the lock
 * =d
 * Acquires the spinlock named LOCK. LOCK must be defined in a SpinlockInfo
 * element. The packet is then pushed through the pipeline and when the
 * processing down the push path is done, releases the lock.
 *
 * =n
 * Ensure that a push path will not be traversed by multiple threads at the same
 * time while SpinlockAcquire/SpinlockRelease work for any path but do not
 * support dropping packets between a SpinlockAcquire and a SpinlockRelease
 *
 * =a SpinlockInfo, SpinlockAcquire, SpinlockRelease
 */

class SpinlockPush : public Element { public:

    SpinlockPush()			: _lock(0) {}
    ~SpinlockPush()			{}

    const char *class_name() const	{ return "SpinlockPush"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    void push(int,Packet *p)	{ _lock->acquire(); output(0).push(p); _lock->release(); }

  private:

    Spinlock *_lock;

};

CLICK_ENDDECLS
#endif
