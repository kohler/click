// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PAD_HH
#define CLICK_PAD_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

Pad

=s modification

*/

class Pad : public Element { public:
    
    Pad();
    ~Pad();
  
    const char *class_name() const		{ return "Pad"; }
    const char *processing() const		{ return AGNOSTIC; }
  
    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
