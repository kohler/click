// -*- c-basic-offset: 4 -*-
#ifndef CLICK_FROMHOST_BSDMODULE_HH
#define CLICK_FROMHOST_BSDMODULE_HH
#include <click/element.hh>
#include <click/notifier.hh>
#include "elements/bsdmodule/anydevice.hh"
CLICK_DECLS

class FromHost : public AnyDevice {

  public:
    FromHost() CLICK_COLD;
    ~FromHost() CLICK_COLD;

    const char *class_name() const	{ return "FromHost"; }
    const char *port_count() const	{ return PORTS_0_1; }
    const char *processing() const	{ return PUSH; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    bool run_task(Task *);
    struct ifqueue *_inq;

  private:
    static const int QSIZE = 511;
    unsigned _burst;
};

CLICK_ENDDECLS
#endif
