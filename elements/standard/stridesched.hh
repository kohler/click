#ifndef STRIDESCHED_HH
#define STRIDESCHED_HH

#include "element.hh"

/*
 * =c
 * StrideSched(ticket_1, ..., ticket_n)
 * =d
 * Has one output and n inputs.  Performs simple packet-based stride
 * scheduling, assigning ticket_i to input_i for each i in 1 to n.
 * 
 * Each time a pull comes in on the output, it pulls on its inputs in the order
 * specified by the stride scheduling queue, until all inputs have been tried
 * or one produces a packet.  If an input does not produce a packet, it is not
 * tried again in the current round (for the current pull on the output) even
 * if it has a very short stride.  This minimizes overhead and ensures that
 * an input that produces a packet, if any, is found as soon as possible,
 * consistently with the stride scheduler ordering.
 * 
 * =a PrioSched, RoundRobinSched
 */

class StrideSched : public Element {
  
  static const int stride1 = 100000;
  static const long long max_pass = 1LL<<61;

  class Client {

    Client *_p;
    Client *_n;
    long long _pass;
    unsigned int _stride;
    int _tickets;
    int _id;
    Client *_list;
    
    void reset_pass(void);

  public:
    
    Client() : _p(0), _n(0), _pass(0), _stride(0), _tickets(-1), _id(-1) {}
    Client(int id, int tickets);
    
    void make_head(void);
    Client *remove_min(void);
    void insert(Client *c);
    void stride(void);
    int id(void) 				{ return _id; }

  };
  
public:
  
  StrideSched();
  ~StrideSched();

  int configure(const String &conf, ErrorHandler *errh);
  void uninitialize(void);
  
  const char *class_name() const		{ return "StrideSched"; }
  Processing default_processing() const		{ return PULL; }
  
  StrideSched *clone() const			{ return new StrideSched; }
  Packet *pull(int port);

private:

  Client *_list;

};

inline
StrideSched::Client::Client(int id, int tickets)
{
  _tickets = tickets;
  _stride = stride1 / tickets;
  _pass = _stride;
  _id = id;
}

inline
void StrideSched::Client::make_head(void)
{
  _p = _n = _list = this;
}

inline void
StrideSched::Client::insert(Client *c)
{
  assert(this == _list);
  Client *x = _n;
  while (x != _list && x->_pass < c->_pass)
    x = x->_n;
  // insert c before x
  c->_n = x;
  c->_p = x->_p;
  c->_p->_n = c;
  x->_p = c;
}

inline StrideSched::Client *
StrideSched::Client::remove_min(void)
{
  assert(this == _list);
  if (_n != this) {
    Client *r = _n;
    _n = r->_n;
    _n->_p = this;
    r->_n = r->_p = r->_list = NULL;
    return r;
  }
  return NULL;
}

inline void
StrideSched::Client::reset_pass(void)
{
  Client *c = _list->_n;
  int pass = c->_pass;
  while (c != _list) {
    c->_pass -= pass;
    c = c->_n;
  }
}

inline void
StrideSched::Client::stride(void)
{
  _pass += _stride;
  if (_pass > max_pass)
    reset_pass();
}

#endif STRIDESCHED_HH
