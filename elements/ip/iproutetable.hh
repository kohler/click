// -*- c-basic-offset: 4 -*-
#ifndef CLICK_IPROUTETABLE_HH
#define CLICK_IPROUTETABLE_HH
#include <click/glue.hh>
#include <click/element.hh>
CLICK_DECLS

/*
=c

IPRouteTable

=s IP, classification

IP routing table superclass

=d

IPRouteTable defines an interface useful for implementing IPv4 route lookup
elements. It parses configuration strings -- see LinearIPLookup for an example
-- and calls virtual functions to add the resulting routes. A default C<push>
function uses those virtual functions to look up routes and output packets
accordingly. There are also some functions useful for implementing handlers.

These four IPRouteTable virtual functions should generally be overridden by
particular routing table elements.

=over 4

=item C<int B<add_route>(IPAddress dst, IPAddress mask, IPAddress gw, int p, ErrorHandler *errh)>

Adds a route sending packets with destination addresses matching C<dst/mask>
to gateway C<gw>, via the element's output port C<p>. Any errors are reported
to C<errh>. Should return 0 on success and negative on failure. The default
implementation reports an error "cannot add routes to this routing table".

=item C<int B<remove_route>(IPAddress dst, IPAddress mask, IPAddress gw, int p, ErrorHandler *errh)>

Removes the route sending packets with destination addresses matching
C<dst/mask> to gateway C<gw>, via the element's output port C<p>. Any errors
are reported to C<errh>. Should return 0 on success and negative on failure.
The default implementation reports an error "cannot delete routes from this
routing table".

=item C<int B<lookup_route>(IPAddress dst, IPAddress &gw_return) const>

Looks up the route associated with address C<dst>. Should set C<gw_return> to
the resulting gateway and return the relevant output port (or negative if
there is no route). The default implementation returns -1.

=item C<String B<dump_routes>() const>

Returns a textual description of the current routing table. The default
implementation returns an empty string.

=back

The following functions, overridden by IPRouteTable, are available for use by
subclasses.

=over 4

=item C<int B<configure>(VectorE<lt>StringE<gt> &conf, ErrorHandler *)>

The default implementation of B<configure> parses C<conf> as a list of routes,
where each route is the space-separated list `C<address/mask [gateway]
output>'. The routes are successively added to the element with B<add_route>.

=item C<void B<push>(int port, Packet *p)>

The default implementation of B<push> uses B<lookup_route> to perform IP
routing lookup. Normally, subclasses implement their own B<push> methods,
avoiding virtual function call overhead.

=item C<static int B<add_route_handler>(const String &, Element *, void *, ErrorHandler *)>

This write handler callback parses its input as an add-route request
and calls B<add_route> with the results. Normally hooked up to the `C<add>'
handler.

=item C<static int B<remove_route_handler>(const String &, Element *, void *, ErrorHandler *)>

This write handler callback parses its input as a remove-route request and
calls B<remove_route> with the results. Normally hooked up to the `C<remove>'
handler.

=item C<static int B<ctrl_handler>(const String &, Element *, void *, ErrorHandler *)>

This write handler callback function parses its input as a route control
request and calls B<add_route> or B<remove_route> as directed. Normally hooked
up to the `C<ctrl>' handler.

=item C<static String B<table_handler>(Element *, void *)>

This read handler callback function returns the element's routing table via
the B<dump_routes> function. Normally hooked up to the `C<table>' handler.

=back

=a StaticIPLookup, LinearIPLookup, RadixIPLookup */

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
