#ifndef DEVIRTUALIZEINFO_HH
#define DEVIRTUALIZEINFO_HH
#include <click/element.hh>

class DevirtualizeInfo : public Element {

 public:

  DevirtualizeInfo();
  ~DevirtualizeInfo();
  
  const char *class_name() const	{ return "DevirtualizeInfo"; }
  DevirtualizeInfo *clone() const	{ return new DevirtualizeInfo; }
  int configure(const Vector<String> &, ErrorHandler *);

};

#endif
