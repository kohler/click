// -*- c-basic-offset: 4 -*-
#ifndef CLICK_THREADSCHED_HH
#define CLICK_THREADSCHED_HH
CLICK_DECLS

class ThreadSched { public:

    ThreadSched()			{ }
    virtual ~ThreadSched()		{ }

    enum { THREAD_PREFERENCE_UNKNOWN = -1000 };
    virtual int initial_thread_preference(Task *, bool);

};

CLICK_ENDDECLS
#endif
