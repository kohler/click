#ifndef CLICK_STRIDESCHED_HH
#define CLICK_STRIDESCHED_HH
#include <click/element.hh>
#include <click/task.hh>

/*
 * =c
 * StrideSched(TICKETS0, ..., TICKETSI<N-1>)
 * =s packet scheduling
 * pulls from stride-scheduled inputs
 * =d
 * Has one output and N inputs.  Performs simple packet-based stride
 * scheduling, assigning TICKETSI<i> to input I<i> for each input.
 * 
 * Each time a pull comes in on the output, it pulls on its inputs in the order
 * specified by the stride scheduling queue, until all inputs have been tried
 * or one produces a packet.  If an input does not produce a packet, it is not
 * tried again in the current round (for the current pull on the output) even
 * if it has a very short stride.  This minimizes overhead and ensures that
 * an input that produces a packet, if any, is found as soon as possible,
 * consistently with the stride scheduler ordering.
 *
 * =h tickets0...ticketsI<N-1> read/write
 * Returns or sets the number of tickets for each input port.
 *
 * =a PrioSched, RoundRobinSched, DRRSched, StrideSwitch
 */

class StrideSched : public Element { protected:
  
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
    
    int id() const 				{ return _id; }
    void set_tickets(int);
    
    void make_head();
    
    void insert(Client *c);
    void remove();
    void stride();

  };
  
  Client *_list;

 public:
  
  StrideSched();
  ~StrideSched();

  const char *class_name() const		{ return "StrideSched"; }
  const char *processing() const		{ return PULL; }
  
  StrideSched *clone() const			{ return new StrideSched; }
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void uninitialize();
  void add_handlers();

  int tickets(int) const;
  int set_tickets(int, int, ErrorHandler *);
  
  Packet *pull(int port);

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

inline void
StrideSched::Client::remove()
{
  _n->_p = _p;
  _p->_n = _n;
  _n = _p = 0;
}

inline void
StrideSched::Client::set_tickets(int tickets)
{
  _tickets = tickets;
  _stride = STRIDE1 / tickets;
}

inline void
StrideSched::Client::stride()
{
  _pass += _stride;
}

#endif
