#ifndef SPECIALIZERINFO_HH
#define SPECIALIZERINFO_HH
#include "element.hh"

class SpecializerInfo : public Element {

 public:

  SpecializerInfo();
  
  const char *class_name() const		{ return "SpecializerInfo"; }
  
  SpecializerInfo *clone() const		{ return new SpecializerInfo; }
  int configure(const String &, ErrorHandler *);

};

#endif
