#ifndef SAMPLEPACKAGEELEMENT_HH
#define SAMPLEPACKAGEELEMENT_HH

/*
 * =c
 * SamplePackageElement()
 * =s
 * demonstrates how to write a package
 * =d
 *
 * This is the only element in the `sample' package. It demonstrates how to
 * write an element that will be placed in a package. It does nothing except
 * report that the package was successfully loaded when it initializes. */

#include <click/element.hh>

class SamplePackageElement : public Element { public:
  
  SamplePackageElement();		// SEE sample.cc FOR CONSTRUCTOR
  ~SamplePackageElement();		// SEE sample.cc FOR DESTRUCTOR
  
  const char *class_name() const	{ return "SamplePackageElement"; }
  const char *processing() const	{ return AGNOSTIC; }
  SamplePackageElement *clone() const	{ return new SamplePackageElement; }

  int initialize(ErrorHandler *);
  
};

#endif
