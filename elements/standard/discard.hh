#ifndef CLICK_DISCARD_HH
#define CLICK_DISCARD_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
=c

Discard

=s basicsources

drops all packets

=d

Discards all packets received on its single input. If used in a pull context,
it initiates pulls whenever packets are available, and listens for activity
notification, such as that available from Queue.

=h count read-only

Returns the number of packets discarded.

=h reset_counts write-only

Resets "count" to 0.

=a Queue */

class Discard : public Element { public:

  Discard();
  ~Discard();

  const char *class_name() const		{ return "Discard"; }
  const char *port_count() const		{ return PORTS_1_0; }
  const char *processing() const		{ return AGNOSTIC; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void push(int, Packet *);
  bool run_task(Task *);

 protected:

  Task _task;
  NotifierSignal _signal;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;

    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
