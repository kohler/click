#include <click/config.h>
#include <click/package.hh>
#include "cpuqueue.hh"
#include <click/error.hh>
#include <click/confparse.hh>

CPUQueue::CPUQueue() : _last(0), _drops(0)
{
  MOD_INC_USE_COUNT;
  set_ninputs(1);
}

CPUQueue::~CPUQueue()
{
  MOD_DEC_USE_COUNT;
}

CPUQueue *
CPUQueue::clone() const
{
  return new CPUQueue;
}

void
CPUQueue::notify_noutputs(int i)
{
  set_noutputs(i < 1 ? 1 : i);
}

int
CPUQueue::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  unsigned new_capacity = 128;
  if (cp_va_parse(conf, this, errh, 
	          cpOptional, 
		  cpUnsigned, "maximum queue length", &new_capacity, 0) < 0) 
    return -1; 
  _capacity = new_capacity;
  return 0;
}
  
int 
CPUQueue::initialize(ErrorHandler *errh)
{
  for (int i=0; i<NUM_CLICK_CPUS; i++) {
    _q[i]._q = new Packet*[_capacity+1];
    _q[i]._head = 0;
    _q[i]._tail = 0;
  }
  _drops = 0;
  _last = 0;
  return 0;
}

void
CPUQueue::uninitialize()
{
  for (int i=0; i<NUM_CLICK_CPUS; i++) {
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
  int n = current->processor;
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
  for (int i=0; i<NUM_CLICK_CPUS; i++) {
    p = deq(n);
    n++;
    if (n == NUM_CLICK_CPUS) n = 0;
    if (p) {
      _last = n;
      return p;
    }
  }
}

String
CPUQueue::read_handler(Element *e, void *thunk)
{
  CPUQueue *q = static_cast<CPUQueue *>(e);
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(q->capacity()) + "\n";
   case 1:
    return String(q->drops()) + "\n";
   default:
    return "";
  }
}

void
CPUQueue::add_handlers()
{
  add_read_handler("capacity", read_handler, (void *)0);
  add_read_handler("drops", read_handler, (void *)1);
}

EXPORT_ELEMENT(CPUQueue)
ELEMENT_MT_SAFE(CPUQueue)

