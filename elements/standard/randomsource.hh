#ifndef RANDOMSOURCE_HH
#define RANDOMSOURCE_HH
#include <click/element.hh>

/*
 * =c
 * RandomSource(LENGTH)
 * =s sources
 * generates random packets whenever scheduled
 * =d
 * Creates packets, of the indicated length, filled with random bytes.
 * =a InfiniteSource
 */

class RandomSource : public Element { protected:
  
  int _length;
  
 public:
  
  RandomSource();
  ~RandomSource(); 
 
  const char *class_name() const		{ return "RandomSource"; }
  const char *processing() const		{ return AGNOSTIC; }
  RandomSource *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  Packet *pull(int);
  Packet *make_packet();
};

#endif
