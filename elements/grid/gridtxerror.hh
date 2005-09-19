#ifndef GRIDTXERR_HH
#define GRIDTXERR_HH

/*
 * =c
 * GridTxError
 * =s Grid
 * Reports packet transmission error to Grid logging infrastructure
 *=d
 * Keywords:
 * =over 8
 * =item LOG
 * GridGenericLogger element.  If no LOG is specified in the configuration, no logging will occur.
 * =a GridLogger
 */

#include <click/element.hh>
CLICK_DECLS

class GridGenericLogger;

class GridTxError : public Element { public:
  
  GridTxError();
  ~GridTxError();

  const char *class_name() const { return "GridTxError"; }
  const char *port_count() const { return PORTS_1_0; }
  const char *processing() const { return PUSH; }

  int initialize(ErrorHandler *);
  int configure(Vector<String> &, ErrorHandler *);
  void push(int, Packet *);

private:
  GridGenericLogger *_log;

};

CLICK_ENDDECLS
#endif
