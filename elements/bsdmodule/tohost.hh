#ifndef CLICK_TOHOST_HH
#define CLICK_TOHOST_HH
#include <click/element.hh>
#include <net/ethernet.h>
#include "elements/bsdmodule/anydevice.hh"

class ToHost : public AnyDevice {

  public:
    ToHost();
    ~ToHost();
    const char *class_name() const      { return "ToHost"; }
    const char *processing() const      { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    void push(int, Packet *);
};

#endif
