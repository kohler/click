// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_TOHOSTSNIFFERS_BSDMODULE_HH
#define CLICK_TOHOSTSNIFFERS_BSDMODULE_HH
#include "elements/bsdmodule/tohost.hh"
CLICK_DECLS

/*
 * ToHostSniffers behaves exactly like ToHost, except that the SNIFFERS
 * keyword argument defaults to true.
 *
 * Based on elements/linuxmodule/tohost.hh.
 */

class ToHostSniffers : public ToHost { public:

    ToHostSniffers() CLICK_COLD;
    ~ToHostSniffers() CLICK_COLD;

    const char *class_name() const		{ return "ToHostSniffers"; }

};

CLICK_ENDDECLS
#endif

