// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATEFLOWS_HH
#define CLICK_AGGREGATEFLOWS_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <click/bighashmap.hh>

/*
=c

AggregateFlows([I<KEYWORDS>])

=s

collects information about TCP flows

=d

Keywords are:

=over 8

=item BIDI

Boolean.

=item PORTS

Boolean.

=back

*/

class AggregateFlows : public Element { public:

    AggregateFlows();
    ~AggregateFlows();

    const char *class_name() const	{ return "AggregateFlows"; }
    AggregateFlows *clone() const	{ return new AggregateFlows; }

    void notify_noutputs(int);
    const char *processing() const	{ return "a/ah"; }
    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    Packet *simple_action(Packet *);
    
  private:

    typedef BigHashMap<IPFlowID, uint32_t> Map;
    Map _tcp_map;
    Map _udp_map;
    
    uint32_t _next;
    bool _bidi;
    bool _ports;

};

#endif
