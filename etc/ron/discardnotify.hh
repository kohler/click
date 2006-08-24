#ifndef DISCARDNOTIFY_HH
#define DISCARDNOTIFY_HH
#include <click/element.hh>
#include <click/task.hh>
#include "../../elements/standard/discard.hh"
#include "queuenotify.hh"
/*
 * =c
 * DiscardNotify
 * =s dropping
 * drops all packets
 * =d
 * Discards all packets received on its single input.
 * If used in a Pull context, it initiates pulls whenever
 * packets are available.
 */

class DiscardNotify : public Discard, public NotifiedElement { public:
  DiscardNotify();
  ~DiscardNotify();

  const char *class_name() const { return "DiscardNotify";}
  int initialize(ErrorHandler *);

  void notify(int signal);
  bool run_task(Task *);
protected:
  bool _data_ready;
};

#endif
