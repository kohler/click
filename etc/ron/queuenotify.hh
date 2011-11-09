#ifndef QUEUENOTIFY_HH
#define QUEUENOTIFY_HH
#include <click/element.hh>
#include <click/bitvector.hh>
#include <click/glue.hh>
#include "elements/standard/notifierqueue.hh"
/*
 * =c
 * QueueNotify
 * QueueNotify(CAPACITY)
 * =s storage
 * stores packets in a FIFO queue
 * =d
 * Stores incoming packets in a first-in-first-out queue.
 * Drops incoming packets if the queue already holds CAPACITY packets.
 * The default for CAPACITY is 1000.
 * =h length read-only
 * Returns the current number of packets in the queue.
 * =h highwater_length read-only
 * Returns the maximum number of packets that have ever been in the queue at once.
 * =h capacity read/write
 * Returns or sets the queue's capacity.
 * =h drops read-only
 * Returns the number of packets dropped by the Queue so far.
 * =h reset_counts write-only
 * When written, resets the C<drops> and C<highwater_length> counters.
 * =h reset write-only
 * When written, drops all packets in the Queue.
 * =a Queue, RED, FrontDropQueue
 */

class NotifiedElement { public:
  virtual void notify(int i);
};

class QueueNotify : public NotifierQueue { public:
  static const int NODATA = 0;
  static const int DATAREADY = 1;

  QueueNotify();

  const char *class_name() const { return "QueueNotify";}
  void *cast(const char *);

  void subscribe_notification(NotifiedElement *e);
  void notify_subscribers(int signal);

  void push(int port, Packet *);
  Packet *pull(int port);


protected:
  Vector<NotifiedElement*> _subscribers;

};


#endif

