// -*- c-basic-offset: 4 -*-
#ifndef CLICK_IPROUTETABLE_HH
#define CLICK_IPROUTETABLE_HH
#include <click/glue.hh>
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * IPRouteTable
 * =s IP, classification
 * ip routing table super class
 * =d
 *
 * IPRouteTable defines the interface each IP routing table lookup
 * element must implement.
 *
 * IPRouteTable expects IP packets with dest IP addr annotations. for
 * each packet, it looks up the dest IP addr annotation in the routing
 * table, replaces the dest IP addr annotation with the new gateway,
 * and pushes the packet out on one of the outputs.
 *
 * Subclasses of IPRouteTable needs to implement four routines:
 * add_route, remove_route, lookup_route, and dump_routes. Replacing
 * annotation and pushing packets around are all taken care of by
 * IPRouteTable. the signatures for the routines that need to be
 * written are:
 *
 * void add_route(IPAddress dst, IPAddress mask, IPAddress gw, int port);
 * void remove_route(IPAddress dst, IPAddress mask);
 * int lookup_route(IPAddress dst, IPAddress &gw);  // returns port
 * String dump_routes();
 *
 * =h ctrl write
 * Take in changes to the routing table, in the format of 
 *
 *    add ip/mask [gw] output
 *    remove ip/mask
 *
 * for example,
 *
 *    add 18.26.4.0/24 18.26.4.1 0
 *
 * says all packets to 18.26.4.0/24 subnet should use gateway
 * 18.26.4.1, and go out on output port 0. and
 *
 *    remove 18.26.4.0/24
 *
 * removes the route.
 *
 * =h look read-only
 * Returns the contents of the routing table.
 *
 * =a StaticIPLookup, RadixIPLookup
 */

class IPRouteTable : public Element { public:

    void *cast(const char *);
    int configure(Vector<String> &, ErrorHandler *);

    virtual int add_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler*);
    virtual int remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    virtual int lookup_route(IPAddress, IPAddress &) const = 0;
    virtual String dump_routes() const;

    void push(int port, Packet *p);

    static int add_route_handler(const String &, Element *, void *, ErrorHandler *);
    static int remove_route_handler(const String &, Element *, void *, ErrorHandler *);
    static int ctrl_handler(const String &, Element *, void *, ErrorHandler *);
    static String table_handler(Element *, void *);

};

CLICK_ENDDECLS
#endif
