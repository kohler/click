#ifndef TODUMP_HH
#define TODUMP_HH
#include "timer.hh"
#include "element.hh"
#include <stdio.h>

/*
 * =c
 * ToDump(FILENAME)
 * =d
 * Writes incoming packets to FILENAME in `tcpdump -w' format. This file
 * can be read by FromDump on a later run.
 *
 * This element is only available at user level.
 * 
 * =a FromDump
 * =a FromBPF
 * =a ToBPF
 */

class ToDump : public Element {
  
  String _filename;
  FILE *_fp;
  
  void write_packet(Packet *);
  
 public:
  
  ToDump();
  ~ToDump();
  
  const char *class_name() const		{ return "ToDump"; }
  ToDump *clone() const;
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void push(int, Packet *);
  void run_scheduled();
  
};

#endif
