#ifndef CLICK_TOHOST_HH
#define CLICK_TOHOST_HH
#include "elements/bsdmodule/tohost.hh"

class ToHost : public AnyDevice {

  public:
    ToHost();
    ~ToHost();
    const char *class_name() const      { return "ToHost"; }
    const char *processing() const      { return PUSH; }
    ToHost *clone() const		{ return new ToHost; }

    int configure(const Vector<String> &, ErrorHandler *);
    void push(int, Packet *);
};

#endif
