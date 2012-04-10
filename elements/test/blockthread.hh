// -*- c-basic-offset: 4 -*-
#ifndef CLICK_BLOCKTHREAD_HH
#define CLICK_BLOCKTHREAD_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

BlockThread()

=s control

provides handler for blocking thread execution

=d

BlockThread's "block" handler can be used to block a Click thread's execution
for a specified time period.  This is only useful for testing at userlevel.

=h block write-only

Write this handler to block execution for a specified number of seconds.

*/

class BlockThread : public Element { public:

    BlockThread();

    const char *class_name() const	{ return "BlockThread"; }

    void add_handlers();

  private:

    static int handler(int, String&, Element*, const Handler*, ErrorHandler*);

};

CLICK_ENDDECLS
#endif
