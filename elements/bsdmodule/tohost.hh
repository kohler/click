#ifndef CLICK_TOHOST_BSDMODULE_HH
#define CLICK_TOHOST_BSDMODULE_HH
#include <click/element.hh>
#include <net/ethernet.h>
#include "elements/bsdmodule/anydevice.hh"
CLICK_DECLS

class ToHost : public AnyDevice {

  public:

    ToHost();
    ~ToHost();

    static void static_initialize();
    static void static_cleanup();

    const char *class_name() const      { return "ToHost"; }
    const char *port_count() const	{ return PORTS_1_0; }
    const char *processing() const      { return PUSH; }
    const char *flags() const		{ return "S2"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_TODEVICE; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void push(int, Packet *);

  private:

    bool _sniffers;
    bool _allow_nonexistent;
    int _drops;

    static String read_handler(Element *, void *);

    friend class ToHostSniffers;
};

CLICK_ENDDECLS
#endif
