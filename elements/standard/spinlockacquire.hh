#ifndef SPINLOCKACQUIRE_HH
#define SPINLOCKACQUIRE_HH
#include <click/element.hh>
#include <click/sync.hh>

/*
 * =c
 * SpinlockAcquire(S)
 * =s
 * acquires spinlock
 * =d
 * Acquires the spinlock named S. S must be defined in a SpinlockInfo element.
 */

class SpinlockAcquire : public Element {
  Spinlock *_lock;
 
public:
  SpinlockAcquire()			{}
  ~SpinlockAcquire()			{}

  const char *class_name() const	{ return "SpinlockAcquire"; }
  const char *processing() const	{ return AGNOSTIC; }
  SpinlockAcquire *clone() const	{ return new SpinlockAcquire; }
  
  void notify_ninputs(int n)		{ set_ninputs(n); }
  void notify_noutputs(int n)		{ set_noutputs(n); }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize()			{ _lock->unref(); }
  
  Packet *simple_action(Packet *p)  	{ _lock->acquire(); return p; }
};

#endif
