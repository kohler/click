#ifndef CLICK_SPINLOCKINFO_HH
#define CLICK_SPINLOCKINFO_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/sync.hh>
CLICK_DECLS

/*
 * =c
 * SpinlockInfo(NAME, ...)
 * =s
 * specifies names of spinlocks
 * =io
 * None
 * =d
 *
 * Lets you use mnemonic names for spinlocks. Each name names a spinlock that
 * the SpinlockAcquire and SpinlockRelease elements can use to reference a
 * spinlock.
 */

class SpinlockInfo : public Element {

  HashMap<String, int> _map;
  Vector<Spinlock *> _spinlocks;

  int add_spinlock(const Vector<String> &, const String &, ErrorHandler *);

public:
  
  SpinlockInfo();
  ~SpinlockInfo();
  const char *class_name() const	{ return "SpinlockInfo"; }
  SpinlockInfo *clone() const		{ return new SpinlockInfo; }
  int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
  int configure(Vector<String> &, ErrorHandler *);
  void cleanup(CleanupStage);
  
  Spinlock *query(const String &, const String &) const;
};

CLICK_ENDDECLS
#endif
