#ifndef CLICK_SPINLOCKRELEASE_HH
#define CLICK_SPINLOCKRELEASE_HH
#include <click/element.hh>
#include <click/sync.hh>

/*
 * =c
 * SpinlockRelease(S)
 * =s
 * releases spinlock
 * =d
 * Releases the spinlock named S. S must be defined in a SpinlockInfo element.
 */

class SpinlockRelease : public Element {
  Spinlock *_lock;
 
public:
  SpinlockRelease()			: _lock(0) {}
  ~SpinlockRelease()			{}

  const char *class_name() const	{ return "SpinlockRelease"; }
  const char *processing() const	{ return AGNOSTIC; }
  SpinlockRelease *clone() const	{ return new SpinlockRelease; }
  
  void notify_ninputs(int n)		{ set_ninputs(n); }
  void notify_noutputs(int n)		{ set_noutputs(n); }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage)		{ if (_lock) _lock->unref(); }
  
  Packet *simple_action(Packet *p)  	{ _lock->release(); return p; }
};

#endif
