#ifndef PROBSPLITTER_HH
#define PROBSPLITTER_HH
#include <click/element.hh>

/*
 * =c
 * ProbSplitter(P)
 * =s classification
 * classifies packets probabilistically
 * =deprecated RandomSample
 * =d
 *
 * This element has been deprecated. Use RandomSample(DROP P) instead.
 *
 * =a RandomSample
 */

class ProbSplitter : public Element { public:
  
  ProbSplitter();
  ~ProbSplitter();

  const char *class_name() const		{ return "ProbSplitter"; }
  ProbSplitter *clone() const			{ return new ProbSplitter; }
  
  int configure(const Vector<String> &, ErrorHandler *);

};

#endif


