#ifndef CLICK_DUPPATH_HH
#define CLICK_DUPPATH_HH
#include <click/element.hh>
CLICK_DECLS

class DupPath : public Element {
  struct {
    Packet **_q;
    unsigned _head;
    unsigned _tail;
  } _q;

  int next_i(int i) const { return (i!=128 ? i+1 : 0); }
  Packet *deq();

 public:

  DupPath() CLICK_COLD;
  ~DupPath() CLICK_COLD;

  const char *class_name() const		{ return "DupPath"; }
  const char *port_count() const		{ return "1/2"; }
  const char *processing() const		{ return "h/hl"; }
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;

  void push(int port, Packet *);
  Packet *pull(int port);
};

CLICK_ENDDECLS
#endif
