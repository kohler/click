// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ENSUREDPDKBUFFER_HH
#define CLICK_ENSUREDPDKBUFFER_HH
#include <click/config.h>
#include <click/element.hh>

CLICK_DECLS

/*
 * =title EnsureDPDKBuffer
 *
 * =c
 *
 * EnsureDPDKBuffer()
 *
 * =d
 *
 * Ensure that all packets passing through this element are using DPDK buffer and not Click or mmap'd buffers. Packet will be copied to a new DPDK buffer if it's not the case.
 *
 * =item FORCE_COPY
 *
 * Always copy packets to a new DPDK buffer
 *
 * =item EXTRA_HEADROOM
 *
 * When a copy is done, add an extra headroom space
 */


class EnsureDPDKBuffer: public Element  {

public:
    EnsureDPDKBuffer() CLICK_COLD;
    ~EnsureDPDKBuffer() CLICK_COLD;

    const char *class_name() const        { return "EnsureDPDKBuffer"; }
    const char *port_count() const        { return PORTS_1_1; }
    const char *processing() const        { return AGNOSTIC; }
    int configure_phase() const {
        return CONFIGURE_PHASE_PRIVILEGED + 3;
    }
    int configure(Vector<String> &conf, ErrorHandler *errh) override CLICK_COLD;
    int initialize(ErrorHandler *errh) override CLICK_COLD;

    Packet* smaction(Packet*);
    Packet* simple_action(Packet*);

private:
    bool _force;
    int _extra_headroom;
    bool _noalloc;
    int _warn_count;
};

CLICK_ENDDECLS
#endif
