#ifndef TODUMP_HH
#define TODUMP_HH
#include <click/timer.hh>
#include <click/element.hh>
#include <stdio.h>

/*
 * =c
 *
 * ToDump(FILENAME)
 *
 * =s sinks
 *
 * writes packets to a tcpdump(1) file
 *
 * =d
 *
 * Writes incoming packets to FILENAME in `tcpdump -w' format. This file
 * can be read by FromDump on a later run.
 *
 * This element is only available at user level.
 * 
 * =a
 *
 * FromDump, FromDevice.u, ToDevice.u, tcpdump(1)
 */

class ToDump : public Element {
  
  String _filename;
  FILE *_fp;
  
  void write_packet(Packet *);
  
 public:
  
  ToDump();
  ~ToDump();
  
  const char *class_name() const		{ return "ToDump"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flags() const			{ return "S2"; }
  ToDump *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void push(int, Packet *);
  void run_scheduled();
  
};

#endif
