
#ifndef DUPPATH_HH
#define DUPPATH_HH

#include <click/element.hh>

class DupPath : public Element {
  struct {
    Packet **_q;
    unsigned _head;
    unsigned _tail;
  } _q;

  int next_i(int i) const { return (i!=128 ? i+1 : 0); }
  Packet *deq();

 public:
  
  DupPath();
  ~DupPath();
  
  const char *class_name() const		{ return "DupPath"; }
  const char *processing() const		{ return "h/hl"; }
  int initialize(ErrorHandler *);
  void uninitialize();
  DupPath *clone() const;
  
  void push(int port, Packet *);
  Packet *pull(int port);
};

#endif
