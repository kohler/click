#ifndef XOKWRITER_HH
#define XOKWRITER_HH
#include "element.hh"
#include "string.hh"


/*
 * =c
 * xokWriter(devname)
 * =d
 * Write packets to the ethernet via xok ethernet interface. Expects packets
 * that already have an ether header. 
 *
 * =a xokReader
 */

class xokWriter : public Element {
  int cardno;

 public:
  
  xokWriter(int cardno=-1);
  xokWriter(const String &ifname);
  ~xokWriter() {}
  
  const char *class_name() const		{ return "xokWriter"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  xokWriter *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  
  void push(int port, Packet *);

  bool wants_packet_upstream() const;
  void run_scheduled();
  
};


#endif XOKWRITER_HH

