#ifndef SUPPRESSER_HH
#define SUPPRESSER_HH
#include "unlimelement.hh"

class Suppressor : public UnlimitedElement {
  
public:
  Suppressor();
  ~Suppressor();
  
  const char *class_name() const		{ return "Suppressor"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  bool unlimited_inputs() const			{ return true; }
  bool unlimited_outputs() const		{ return true; }
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  
  Suppressor* clone() const;
  int initialize(Router *, ErrorHandler *);
  
  void push(int port, Packet *p);
  Packet *pull(int port);

  static String read_status(Element* f, void *);
  void add_handlers(HandlerRegistry *);
  
  bool suppressed(int output) const { return FD_ISSET(output, &_suppressed); }
  void suppress(int output) { FD_SET(output, &_suppressed); }
  void allow(int output)    { FD_CLR(output, &_suppressed); }
  void allow_all()          { FD_ZERO(&_suppressed); }

  bool set(int output, bool suppressed);

private:
  fd_set _suppressed;
};

#endif
