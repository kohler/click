#ifndef CLICK_STATELESSTCPRESP_HH
#define CLICK_STATELESSTCPRESP_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

class StatelessTCPResponder : public Element { public:

    StatelessTCPResponder()		{ }
    ~StatelessTCPResponder()		{ }

    const char *class_name() const	{ return "StatelessTCPResponder"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    String _data;

};

CLICK_ENDDECLS
#endif
