#ifndef INFINITESOURCE_HH
#define INFINITESOURCE_HH
#include "element.hh"

class InfiniteSource : public Element {
  
  String _data;
  int _count;
  int _limit;
  int _total;
  
 public:
  
  InfiniteSource();
  
  const char *class_name() const		{ return "InfiniteSource"; }
  Processing default_processing() const		{ return PUSH; }
  
  InfiniteSource *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void run_scheduled();
  
};

#endif
