#ifndef TODUMP_HH
#define TODUMP_HH
#include "timer.hh"
#include "element.hh"
#include <stdio.h>

/*
 * =c
 * ToDump(filename)
 * =c
 * Writes incoming packets to filename in tcpdump -w format.
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
  Timer _timer;
  
 public:
  
  ToDump();
  ToDump(String filename);
  ~ToDump();
  
  const char *class_name() const		{ return "ToDump"; }
  Processing default_processing() const	{ return PULL; }
  ToDump *clone() const;
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
