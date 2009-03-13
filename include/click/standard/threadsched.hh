// -*- c-basic-offset: 4 -*-
#ifndef CLICK_THREADSCHED_HH
#define CLICK_THREADSCHED_HH
CLICK_DECLS

class ThreadSched { public:

    enum { THREAD_QUIESCENT = -1, THREAD_STRONG_UNSCHEDULE = -2,
	   THREAD_UNKNOWN = -1000 };

    ThreadSched()			{ }
    virtual ~ThreadSched()		{ }

    virtual int initial_home_thread_id(Element *owner, Task *t, bool scheduled);

};

CLICK_ENDDECLS
#endif
