#include <click/config.h>
#include <click/error.hh>
#include <click/click_ip.h>
#include "duppath.hh"

DupPath::DupPath()
  : Element(1,2)
{
  MOD_INC_USE_COUNT;
  set_ninputs(1);
}

DupPath::~DupPath()
{
  MOD_DEC_USE_COUNT;
}

DupPath *
DupPath::clone() const
{
  return new DupPath;
}

int 
DupPath::initialize(ErrorHandler *)
{
  _q._q = new Packet*[129];
  _q._head = 0;
  _q._tail = 0;
  return 0;
}

void
DupPath::uninitialize()
{
  for (unsigned j = _q._head; j != _q._tail; j = next_i(j))
    _q._q[j]->kill();
  delete[] _q._q;
  _q._q = 0;
}

inline Packet *
DupPath::deq()
{
  if (_q._head != _q._tail) {
    Packet *p = _q._q[_q._head];
    assert(p);
    _q._head = next_i(_q._head);
    return p;
  } else
    return 0;
}

void
DupPath::push(int, Packet *p)
{
  unsigned d = ntohl(p->ip_header()->ip_src.s_addr);
  if ((d ^ (d>>4)) & 1) {
    unsigned next = next_i(_q._tail);
    if (next != _q._head) {
      _q._q[_q._tail] = p;
      _q._tail = next;
    } else
      p->kill();
  } else
    output(0).push(p);
}

Packet *
DupPath::pull(int)
{
  return deq();
}

EXPORT_ELEMENT(DupPath)
ELEMENT_MT_SAFE(DupPath)

