#ifndef CLICK_TOHOST_BSDMODULE_HH
#define CLICK_TOHOST_BSDMODULE_HH
#include <click/element.hh>
#include <net/ethernet.h>
#include "elements/bsdmodule/anydevice.hh"
CLICK_DECLS

class ToHost : public AnyDevice {

  public:

    ToHost() CLICK_COLD;
    ~ToHost() CLICK_COLD;

    static void static_initialize();
    static void static_cleanup();

    const char *class_name() const      { return "ToHost"; }
    const char *port_count() const	{ return PORTS_1_0; }
    const char *processing() const      { return PUSH; }
    const char *flags() const		{ return "S2"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_TODEVICE; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void push(int, Packet *);

  private:

    bool _sniffers;
    bool _allow_nonexistent;
    int _drops;

    static String read_handler(Element *, void *) CLICK_COLD;

    friend class ToHostSniffers;
};

CLICK_ENDDECLS
#endif
