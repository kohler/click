#ifndef GRIDTXERR_HH
#define GRIDTXERR_HH


/*
 * =c
 * GridTxError
 * =s Grid
 * Reports packet transmission error to Grid logging infrastructure
 */

#include <click/element.hh>

class GridTxError : public Element { public:
  
  GridTxError();
  ~GridTxError();

  const char *class_name() const { return "GridTxError"; }
  const char *processing() const { return PUSH; }

  GridTxError *clone() const { return new GridTxError; }
  int initialize(ErrorHandler *);
  
  void push(int, Packet *);

};

#endif
