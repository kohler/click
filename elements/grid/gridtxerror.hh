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
 * GridLogger element.  If no LOG is specified in the configuration, no logging will occur.
 * =a GridLogger
 */

#include <click/element.hh>
#include "gridlogger.hh"
CLICK_DECLS

class GridTxError : public Element { public:
  
  GridTxError();
  ~GridTxError();

  const char *class_name() const { return "GridTxError"; }
  const char *processing() const { return PUSH; }

  GridTxError *clone() const { return new GridTxError; }
  int initialize(ErrorHandler *);
  int configure(Vector<String> &, ErrorHandler *);
  void push(int, Packet *);

private:
  GridLogger *_log;

};

CLICK_ENDDECLS
#endif
