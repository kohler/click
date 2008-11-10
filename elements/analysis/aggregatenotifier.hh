// -*- c-basic-offset: 4 -*-
#ifndef CLICK_AGGREGATENOTIFIER_HH
#define CLICK_AGGREGATENOTIFIER_HH
#include <click/vector.hh>
CLICK_DECLS
class Packet;

class AggregateListener { public:

    AggregateListener()			{ }
    virtual ~AggregateListener()	{ }

    enum AggregateEvent { NEW_AGG, DELETE_AGG };
    virtual void aggregate_notify(uint32_t, AggregateEvent, const Packet *);

};

class AggregateNotifier { public:

    AggregateNotifier()			{ }
    ~AggregateNotifier()		{ }

    void add_listener(AggregateListener *);
    void remove_listener(AggregateListener *);

    void notify(uint32_t, AggregateListener::AggregateEvent, const Packet *) const;

  private:

    Vector<AggregateListener *> _listeners;

};

inline void
AggregateNotifier::notify(uint32_t agg, AggregateListener::AggregateEvent e, const Packet *p) const
{
    for (int i = 0; i < _listeners.size(); i++)
	_listeners[i]->aggregate_notify(agg, e, p);
}

CLICK_ENDDECLS
#endif
