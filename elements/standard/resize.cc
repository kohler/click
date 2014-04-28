// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * resize.{cc,hh} -- adjust packet length
 */

#include <click/config.h>
#include "resize.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
CLICK_DECLS

Resize::Resize()
{
}


int
Resize::configure(Vector<String> &conf, ErrorHandler *errh)
{
    // Must initialize these here, rather than constructor, in case of live reconfiguration.
    _head = 0;
    _tail = 0;
    return Args(conf, this, errh)
        .read_p("HEAD", _head)
        .read_p("TAIL", _tail)
        .complete();
}


static inline uint32_t check_length(uint32_t amount, uint32_t max)
{
    if (amount > max) return max;
    return amount;
}


Packet *
Resize::simple_action(Packet *p)
{
    if (_head > 0) {
        p = p->nonunique_push(_head);
        if (!p) return NULL;
    }
    if (_head < 0) {
        p->pull(check_length(-_head, p->length()));
    }
    if (_tail > 0) {
        p = p->nonunique_put(_tail);
        if (!p) return NULL;
    }
    if (_tail < 0) {
        p->take(check_length(-_tail, p->length()));
    }
    return p;
}


void
Resize::add_handlers()
{
    add_data_handlers("head", Handler::OP_READ | Handler::OP_WRITE, &_head);
    add_data_handlers("tail", Handler::OP_READ | Handler::OP_WRITE, &_tail);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Resize)
ELEMENT_MT_SAFE(Resize)
