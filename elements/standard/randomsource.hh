#ifndef RANDOMSOURCE_HH
#define RANDOMSOURCE_HH
#include "element.hh"

/*
 * =c
 * RandomSource(LENGTH)
 * =s
 * generates random packets whenever scheduled
 * V<sources>
 * =d
 * Creates packets, of the indicated length, filled with random bytes.
 * =a InfiniteSource
 */

class RandomSource : public Element { protected:
  
  int _length;
  
 public:
  
  RandomSource();
  
  const char *class_name() const		{ return "RandomSource"; }
  const char *processing() const		{ return PUSH; }
  RandomSource *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
