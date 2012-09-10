#ifndef CLICK_SPINLOCKINFO_HH
#define CLICK_SPINLOCKINFO_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/hashtable.hh>
#include <click/sync.hh>
CLICK_DECLS

/*
 * =c
 * SpinlockInfo(NAME, ...)
 * =s threads
 * specifies names of spinlocks
 * =d
 *
 * Lets you use mnemonic names for spinlocks. Each name names a spinlock that
 * the SpinlockAcquire and SpinlockRelease elements can use to reference a
 * spinlock.
 * =a SpinlockAcquire, SpinlockRelease
 */

class SpinlockInfo : public Element { public:

    SpinlockInfo() CLICK_COLD;
    ~SpinlockInfo() CLICK_COLD;

    const char *class_name() const	{ return "SpinlockInfo"; }
    int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  private:

    Vector<Spinlock> _spinlocks;

};

CLICK_ENDDECLS
#endif
