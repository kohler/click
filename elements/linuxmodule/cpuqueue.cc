#include <click/config.h>
#include "cpuqueue.hh"
#include <click/error.hh>
#include <click/args.hh>

CPUQueue::CPUQueue()
  : _last(0), _drops(0)
{
  memset(&_q, 0, sizeof(_q));
}

CPUQueue::~CPUQueue()
{
}

int
CPUQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (NR_CPUS > 256)
        return errh->error("too many CPUs for CPUQueue");
    unsigned new_capacity = 128;
    if (Args(conf, this, errh)
	.read_p("CAPACITY", new_capacity)
	.complete() < 0)
	return -1;
    _capacity = new_capacity;
    return 0;
}

int
CPUQueue::initialize(ErrorHandler *errh)
{
  for (int i=0; i<NR_CPUS; i++)
    if (!(_q[i]._q = new Packet*[_capacity+1]))
      return errh->error("out of memory!");
  _drops = 0;
  _last = 0;
  return 0;
}

void
CPUQueue::cleanup(CleanupStage)
{
  for (int i=0; i<NR_CPUS; i++) {
    for (int j = _q[i]._head; j != _q[i]._tail; j = next_i(j))
      _q[i]._q[j]->kill();
    delete[] _q[i]._q;
    _q[i]._q = 0;
  }
}

inline Packet *
CPUQueue::deq(int n)
{
  if (_q[n]._head != _q[n]._tail) {
    Packet *p = _q[n]._q[_q[n]._head];
    _q[n]._head = next_i(_q[n]._head);
    return p;
  } else
    return 0;
}

void
CPUQueue::push(int, Packet *p)
{
    int n = click_current_processor();
    int next = next_i(_q[n]._tail);
    if (next != _q[n]._head) {
	_q[n]._q[_q[n]._tail] = p;
	_q[n]._tail = next;
    } else {
	p->kill();
	_drops++;
    }
}

Packet *
CPUQueue::pull(int port)
{
    int n = _last;
    Packet *p = 0;
    for (int i = 0; i < NR_CPUS; i++) {
	p = deq(n);
	n++;
	if (n == NR_CPUS)
	    n = 0;
	if (p) {
	    _last = n;
	    return p;
	}
    }
    return 0;
}

String
CPUQueue::read_handler(Element *e, void *thunk)
{
  CPUQueue *q = static_cast<CPUQueue *>(e);
  switch (reinterpret_cast<intptr_t>(thunk)) {
   case 0:
    return String(q->capacity());
   case 1:
    return String(q->drops());
   default:
    return "";
  }
}

void
CPUQueue::add_handlers()
{
  add_read_handler("capacity", read_handler, 0);
  add_read_handler("drops", read_handler, 1);
}

ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(CPUQueue)
ELEMENT_MT_SAFE(CPUQueue)
