// -*- c-basic-offset: 4 -*-
#ifndef CLICK_IP6ROUTETABLE_HH
#define CLICK_IP6ROUTETABLE_HH
#include <click/glue.hh>
#include <click/element.hh>
CLICK_DECLS

class IP6RouteTable : public Element { public:

    void* cast(const char*);

    virtual int add_route(IP6Address, IP6Address, IP6Address, int, ErrorHandler *);
    virtual int remove_route(IP6Address, IP6Address, ErrorHandler *);
    virtual String dump_routes();

    static int add_route_handler(const String&, Element*, void*, ErrorHandler*);
    static int remove_route_handler(const String&, Element*, void*, ErrorHandler*);
    static int ctrl_handler(const String&, Element*, void*, ErrorHandler*);
    static String table_handler(Element*, void*);

};

CLICK_ENDDECLS
#endif
