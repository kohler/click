#ifndef CLICK_DEVIRTUALIZEINFO_HH
#define CLICK_DEVIRTUALIZEINFO_HH
#include <click/element.hh>
CLICK_DECLS

class DevirtualizeInfo : public Element {

 public:

  DevirtualizeInfo();
  ~DevirtualizeInfo();
  
  const char *class_name() const	{ return "DevirtualizeInfo"; }
  DevirtualizeInfo *clone() const	{ return new DevirtualizeInfo; }
  int configure(Vector<String> &, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
