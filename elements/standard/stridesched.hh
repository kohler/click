#ifndef STRIDESCHED_HH
#define STRIDESCHED_HH

#include "element.hh"

/*
 * =c
 * StrideSched(TICKET1, ..., TICKET<i>n</i>)
 * =d
 * Has one output and n inputs.  Performs simple packet-based stride
 * scheduling, assigning TICKET<i>i</i> to input <i>i</i> for each input.
 * 
 * Each time a pull comes in on the output, it pulls on its inputs in the order
 * specified by the stride scheduling queue, until all inputs have been tried
 * or one produces a packet.  If an input does not produce a packet, it is not
 * tried again in the current round (for the current pull on the output) even
 * if it has a very short stride.  This minimizes overhead and ensures that
 * an input that produces a packet, if any, is found as soon as possible,
 * consistently with the stride scheduler ordering.
 * 
 * =a PrioSched
 * =a RoundRobinSched
 */

class StrideSched : public Element {
  
  static const unsigned STRIDE1 = 1U<<16;
  static const int MAX_TICKETS = 1U<<15;

  struct Client {

    Client *_p;
    Client *_n;
    unsigned _pass;
    unsigned _stride;
    int _tickets;
    int _id;
    Client *_list;
    
    Client() : _p(0), _n(0), _pass(0), _stride(0), _tickets(-1), _id(-1) {}
    Client(int id, int tickets);
    
    void make_head();
    Client *remove_min();
    void insert(Client *c);
    void stride();
    int id() const 				{ return _id; }

  };
  
public:
  
  StrideSched();
  ~StrideSched();

  int configure(const String &conf, ErrorHandler *errh);
  void uninitialize();
  
  const char *class_name() const		{ return "StrideSched"; }
  const char *processing() const		{ return PULL; }
  
  StrideSched *clone() const			{ return new StrideSched; }
  Packet *pull(int port);

private:

  Client *_list;

};

inline
StrideSched::Client::Client(int id, int tickets)
{
  _tickets = tickets;
  _stride = STRIDE1 / tickets;
  _pass = _stride;
  _id = id;
}

inline
void StrideSched::Client::make_head()
{
  _p = _n = _list = this;
}

inline void
StrideSched::Client::insert(Client *c)
{
  assert(this == _list);
  Client *x = _n;
  while (x != _list && PASS_GT(c->_pass, x->_pass))
    x = x->_n;
  // insert c before x
  c->_n = x;
  c->_p = x->_p;
  c->_p->_n = c;
  x->_p = c;
}

inline StrideSched::Client *
StrideSched::Client::remove_min()
{
  assert(this == _list);
  if (_n != this) {
    Client *r = _n;
    _n = r->_n;
    _n->_p = this;
    r->_n = r->_p = r->_list = 0;
    return r;
  }
  return 0;
}

inline void
StrideSched::Client::stride()
{
  _pass += _stride;
}

#endif STRIDESCHED_HH
