
#include <click/config.h>
#include "msqueue.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>

#define PREFETCH    1
#define BATCHING    1
#define BATCH_SZ    7
#define BATCH_FORCE 128


MSQueue::MSQueue()
  : Element(1, 1), _q(0)
{
  MOD_INC_USE_COUNT;
  _drops = 0;
}

MSQueue::~MSQueue()
{
  MOD_DEC_USE_COUNT;
  if (_q) uninitialize();
}

void *
MSQueue::cast(const char *n)
{
  if (strcmp(n, "Storage") == 0)
    return (Storage *)this;
  else if (strcmp(n, "MSQueue") == 0)
    return (Element *)this;
  else
    return 0;
}

int
MSQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int new_capacity = 128;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpUnsigned, "maximum queue length", &new_capacity,
		  0) < 0)
    return -1;
  _capacity = new_capacity;
  return 0;
}

int
MSQueue::initialize(ErrorHandler *errh)
{
  assert(!_q);
  _q = new Packet *[_capacity + 1];
  if (_q == 0)
    return errh->error("out of memory");

  for(int i=0; i<=_capacity; i++) _q[i] = 0;
  _head = 0;
  _tail = 0;

  _can_pull = false;
  _pulls = 0;
  return 0;
}

void
MSQueue::uninitialize()
{
  for (int i = 0; i <= _capacity; i++) {
    if (_q[i] != 0) {
      _q[i]->kill();
      _q[i] = 0;
    }
  }
  delete[] _q;
  _q = 0;
}

void
MSQueue::push(int, Packet *p)
{
  uint32_t t, n;
  do {
    t = _tail.value();
    n = next_i(t);
    if (n == _head.value()) {
      _drops++;
      p->kill();
      return;
    }
  } while (_tail.compare_and_swap(t, n) != t);
  
  _q[t] = p;
}

Packet *
MSQueue::pull(int)
{
#if BATCHING
  if (size() > BATCH_SZ) 
    _can_pull = true;
  if (_can_pull || _pulls >= BATCH_FORCE) {
#endif
    uint32_t h = _head.value();
    if (h != _tail.value() && _q[h] != 0) {
      Packet *p = _q[h];
      _q[h] = 0;
      _head = next_i(h);
#if PREFETCH
#ifdef __KERNEL__
#if __i386__ && HAVE_INTEL_CPU
      h = _head.value();
      if (_q[h] != 0) prefetch_packet(_q[h]);
#endif
#endif
#endif
      return p;
    }
#if BATCHING
    else {
      _pulls = 0;
      _can_pull = false;
    }
  } else
    _pulls++;
#endif

  return 0;
}

String
MSQueue::read_handler(Element *e, void *thunk)
{
  MSQueue *q = static_cast<MSQueue *>(e);
  int which = reinterpret_cast<int>(thunk);
  switch (which) {
   case 0:
    return String(q->size()) + "\n";
   case 1:
    return String(q->capacity()) + "\n";
   case 2:
    return String(q->drops()) + "\n";
   default:
    return "";
  }
}

void
MSQueue::add_handlers()
{
  add_read_handler("length", read_handler, (void *)0);
  add_read_handler("capacity", read_handler, (void *)1);
  add_read_handler("drops", read_handler, (void *)2);
}

EXPORT_ELEMENT(MSQueue)
