#ifndef CLICK_ERRORTHRESHOLD_HH
#define CLICK_ERRORTHRESHOLD_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS


class ErrorThreshold : public Element { public:
  
  ErrorThreshold();
  ~ErrorThreshold();
  
  const char *class_name() const		{ return "ErrorThreshold"; }
  const char *processing() const		{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void push (int port, Packet *p_in);

  void add_handlers();
  void notify_noutputs(int n);
  unsigned _correct_threshold;
  unsigned _length;
};

CLICK_ENDDECLS
#endif
