#ifndef CLICK_SPINLOCKRELEASE_HH
#define CLICK_SPINLOCKRELEASE_HH
#include <click/element.hh>
#include <click/sync.hh>
CLICK_DECLS

/*
 * =c
 * SpinlockRelease(LOCK)
 * =s smpclick
 * releases spinlock
 * =d
 * Releases the spinlock named LOCK. LOCK must be defined in a SpinlockInfo element.
 */

class SpinlockRelease : public Element {
  Spinlock *_lock;
 
public:
  SpinlockRelease()			: _lock(0) {}
  ~SpinlockRelease()			{}

  const char *class_name() const	{ return "SpinlockRelease"; }
  const char *port_count() const	{ return "-/-"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *p)  	{ _lock->release(); return p; }
  
};

CLICK_ENDDECLS
#endif
