#ifndef CLICK_SPINLOCKRELEASE_HH
#define CLICK_SPINLOCKRELEASE_HH
#include <click/element.hh>
#include <click/sync.hh>
CLICK_DECLS

/*
 * =c
 * SpinlockRelease(LOCK)
 * =s threads
 * releases spinlock
 * =d
 * Releases the spinlock named LOCK. LOCK must be defined in a SpinlockInfo
 * element.
 * =a SpinlockInfo, SpinlockAcquire
 */

class SpinlockRelease : public Element { public:

    SpinlockRelease()			: _lock(0) {}
    ~SpinlockRelease()			{}

    const char *class_name() const	{ return "SpinlockRelease"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *p)	{ _lock->release(); return p; }

  private:

    Spinlock *_lock;

};

CLICK_ENDDECLS
#endif
