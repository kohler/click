/*
 * queue.{cc,hh} -- queue element
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "queue.hh"
#include "confparse.hh"
#include "error.hh"
#include "router.hh"
#include "elemfilter.hh"

Queue::Queue()
  : Storage(1, 1), _q(0)
{
}

Queue::~Queue()
{
  if (_q) uninitialize();
}

bool
Storage::is_a(const char *n) const
{
  return strcmp(n, "Storage") == 0;
}

bool
Queue::is_a(const char *n) const
{
  return strcmp(n, "Queue") == 0 || Storage::is_a(n);
}

int
Queue::configure(const String &conf, ErrorHandler *errh)
{
  int new_max = 1000;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "maximum queue length", &new_max,
		  0) < 0)
    return -1;
  _max = new_max;
  return 0;
}

int
Queue::initialize(ErrorHandler *errh)
{
  _pullers.clear();
  _puller1 = 0;
  
  WantsPacketUpstreamElementFilter ppff;
  if (router()->downstream_elements(this, 0, &ppff, _pullers) < 0)
    return -1;
  ppff.filter(_pullers);

  if (_pullers.size() == 1)
    _puller1 = _pullers[0];
  
  assert(!_q);
  _q = new Packet *[_max + 1];
  if (_q == 0)
    return errh->error("out of memory");

  _empty_jiffies = click_jiffies();
  _head = _tail = 0;
  _drops = 0;
  _max_length = 0;
  return 0;
}

int
Queue::live_reconfigure(const String &conf, ErrorHandler *errh)
{
  // change the maximum queue length at runtime
  int old_max = _max;
  if (configure(conf, errh) < 0)
    return -1;
  if (_max == old_max)
    return 0;
  int new_max = _max;
  _max = old_max;
  
  Packet **new_q = new Packet *[new_max + 1];
  if (new_q == 0)
    return errh->error("out of memory");
  
  int i, j;
  for (i = _head, j = 0; i != _tail; i = next_i(i)) {
    new_q[j++] = _q[i];
    if (j == new_max) break;
  }
  for (; i != _tail; i = next_i(i))
    _q[i]->kill();
  
  delete[] _q;
  _q = new_q;
  _head = 0;
  _tail = j;
  _max = new_max;
  return 0;
}

void
Queue::uninitialize()
{
  for (int i = _head; i != _tail; i = next_i(i))
    _q[i]->kill();
  delete[] _q;
  _q = 0;
}

void
Queue::push(int, Packet *packet)
{
  assert(packet);

  // inline Queue::enq() for speed
  int next = next_i(_tail);
  
  // should this stuff be in Queue::enq?
  if (next != _head) {
    bool was_empty = (_head == _tail);
    _q[_tail] = packet;
    _tail = next;

    /* comment is now a lie --  the problem is the same, the
       solution is different; FromDevice reschedules itself
       on busy, which means it will be run "soon", possibly
       sonner than if we relied on queue length passing a 16
       packet length boundary
    // The Linux net_bh() code processes all queued incoming
    // packets before checking whether output devices have
    // gone idle. Under high load this could leave outputs idle
    // even though packets are Queued. So cause output idleness
    // every 16 packets as well as when we go non-empty. */
    if (was_empty) {
      if (_puller1)
        _puller1->schedule_tail(); // put on the work list.
      else {
	int n = _pullers.size();
	for (int i = 0; i < n; i++)
	  _pullers[i]->schedule_tail();
      }
    }
    
    int s = size();
    if (s > _max_length)
      _max_length = s;
    
  } else {
    // if (!(_drops % 100))
    if (_drops == 0)
      click_chatter("Queue %s overflow", String(id()).cc());
    _drops++;
    packet->kill();
  }
}


Packet *
Queue::pull(int)
{
  return deq();
}

static String
queue_read_length(Element *f, void *thunk)
{
  Queue *q = (Queue *)f;
  if (thunk)
    return String(q->capacity()) + "\n";
  else
    return String(q->size()) + " current\n" +
      String(q->max_length()) + " highest seen\n";
}

static String
queue_read_drops(Element *f, void *)
{
  Queue *q = (Queue *)f;
  return String(q->drops()) + "\n";
}

void
Queue::add_handlers(HandlerRegistry *fcr)
{
  fcr->add_read("length", queue_read_length, (void *)0);
  fcr->add_read_write("max_length", queue_read_length, (void *)1,
		      reconfigure_write_handler, (void *)0);
  fcr->add_read("drops", queue_read_drops, 0);
}

ELEMENT_PROVIDES(Storage)
EXPORT_ELEMENT(Queue)
