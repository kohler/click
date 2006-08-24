// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMHANDLER_HH
#define CLICK_FROMHANDLER_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
CLICK_DECLS
class HandlerCall;

class FromHandler : public Element { public:

    FromHandler();
    ~FromHandler();

    const char *class_name() const		{ return "FromHandler"; }
    const char *port_count() const		{ return "0/1-2"; }
    const char *processing() const		{ return "a/ah"; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();

    void set_active(bool);
    bool run_task(Task *);
    static int write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh);
    static String read_handler(Element *e, void *thunk);

  private:
    bool get_packet();
    bool _active;    
    Task _task;
    String _handler;
    Timestamp _start;
    Timestamp _end;
};

CLICK_ENDDECLS
#endif
