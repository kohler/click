#ifndef SAMPLEPACKAGEELEMENT_HH
#define SAMPLEPACKAGEELEMENT_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SamplePackageElement()
 * =s debugging
 * demonstrates how to write a package
 * =d
 *
 * This is the only element in the `sample' package. It demonstrates how to
 * write an element that will be placed in a package. It does nothing except
 * report that the package was successfully loaded when it initializes. */

class SamplePackageElement : public Element { public:

    SamplePackageElement();		// SEE sample.cc FOR CONSTRUCTOR
    ~SamplePackageElement();		// SEE sample.cc FOR DESTRUCTOR

    const char *class_name() const	{ return "SamplePackageElement"; }

    int initialize(ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
