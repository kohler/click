// -*- c-basic-offset: 4 -*-
#ifndef CLICK_IPSECROUTETABLE_HH
#define CLICK_IPSECROUTETABLE_HH
#include <click/glue.hh>
#include <click/element.hh>
#include "satable.hh"
#include "sadatatuple.hh"
CLICK_DECLS

/*
=c

IPsecRouteTable

=s iproute

IP routing table superclass

=d

IPsecRouteTable defines an interface useful for implementing IPv4 route lookup
elements. It parses configuration strings -- see LinearIPLookup for an example
-- and calls virtual functions to add the resulting routes. A default C<push>
function uses those virtual functions to look up routes and output packets
accordingly. There are also some functions useful for implementing handlers.
IPsecRouteTable also supports IPsec ESP tunnels between gateways.



=head1 PERFORMANCE

Click provides several elements that implement all or part of the IPsecRouteTable
interface.  Marko Zec has compared their performance, in terms of lookup speed
and memory size, for full BGP feeds; here are the results.

Methodology: 2.8GHz Pentium P4 CPU, 521K L2 cache, FreeBSD 4.10, userspace
Click.  RangeIPsecLookup has two lookup tables, the larger of which is used only
at update time.  The "warm cache" numbers perform the same lookup several
times in a loop to populate the cache; only the last lookup's performance is
reported.

              ICSI BGP dump, 150700 routes, 2 next-hops

         Element      | cycles  | lookups | setup | lookup
                      | /lookup | /sec    | time  | tbl. size
     -----------------+---------+---------+-------+----------------
     RadixIPLookup    |   1025  |  2.73M  | 0.59s |  5.8 MB
     DirectIPsecLookup   |    432  |  6.48M  | 0.74s | 33   MB
     RangeIPsecLookup    |    279  | 10.0 M  | 0.83s |  0.21MB (+33MB)
       " (warm cache) |     44  | 63.6 M  |   "   |    "       "

           routeviews.org dump, 167000 routes, 52 nexthops

         Element      | cycles  | lookups | setup | lookup
                      | /lookup | /sec    | time  | tbl. size
     -----------------+---------+---------+-------+----------------
     RadixIPLookup    |   1095  |  2.55M  | 0.67s |  6.6 MB
     DirectIPsecLookup   |    434  |  6.45M  | 0.77s | 33   MB
     RangeIPsecLookup    |    508  |  5.51M  | 0.88s |  0.51MB (+33MB)
       " (warm cache) |     61  | 45.9 M  |   "   |    "       "

The RadixIPLookup, DirectIPsecLookup, and RangeIPsecLookup elements are well suited
for implementing large tables.  We also provide the LinearIPLookup,
StaticIPLookup, and SortedIPLookup elements; they are simple, but their O(N)
lookup speed is orders of magnitude slower.  RadixIPLookup or DirectIPsecLookup
should be preferred for almost all purposes.

           1500-entry fraction of the ICSI BGP dump

         Method     | cycles  | lookups | setup | lookup
                    | /lookup | /sec    | time  | tbl. size
     ---------------+---------+---------+-------+----------
     LinearIPLookup |  12000  |  233K   |  10s  |   29 KB
     StaticIPLookup |  12000  |  233K   |  10s  |   29 KB
     SortedIPLookup |  12500  |  224K   |  38s  |   29 KB

=head1 INTERFACE

These four IPsecRouteTable virtual functions should generally be overridden by
particular routing table elements.

=over 4

=item C<int B<add_route>(const IPsecRoute& r, bool set, IPsecRoute* old_route, ErrorHandler *errh)>

Add a route sending packets with destination addresses matching
C<r.addr/r.mask> to gateway C<r.gw>, via output port C<r.port>.  If a route
for this exact prefix already exists, then the behavior depends on C<set>.  If
C<set> is true, then any existing route is silently overwritten (after
possibly being stored in C<*old_route>); if C<set> is false, the function
should return C<-EEXIST>.  Report errors to C<errh>.  Should return 0 on
success and negative on failure.  The default implementation reports an error
"cannot add routes to this routing table".

=item C<int B<remove_route>(const IPsecRoute& r, IPsecRoute* old_route, ErrorHandler *errh)>

Removes the route sending packets with destination addresses matching
C<r.addr/r.mask> to gateway C<r.gw>, via the element's output port C<r.port>.
All four fields must match, unless C<r.port> is less than 0, in which case
only C<r.addr/r.mask> must match.  If no route for that prefix exists, the
function should return C<-ENOENT>; otherwise, the old route should be stored
in C<*old_route> (assuming it's not null).  Any errors are reported to
C<errh>.  Should return 0 on success and negative on failure.  The default
implementation reports an error "cannot delete routes from this routing
table".

=item C<int B<lookup_route>(IPAddress dst, IPAddress &gw_return) const>

Looks up the route associated with address C<dst>. Should set C<gw_return> to
the resulting gateway and return the relevant output port (or negative if
there is no route). The default implementation returns -1.

=item C<String B<dump_routes>()>

Returns a textual description of the current routing table. The default
implementation returns an empty string.

=back

The following functions, overridden by IPsecRouteTable, are available for use by
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

=head 1 IPSEC ESP SUPPORT

 Ports 0 and 1 must be connected to the proper IPSEC modules that handle incoming tunneled traffic and outgoing
tunneled traffic accordingly. All the routing table entries that refer to an IPSEC tunnel must use these ports respectively. Routing table entries that refer to an IPSEC ESP tunnel must have the following entries:
|SPI| |128-BIT ENCRYPTION_KEY| |128-BIT AUTHENTICATION_KEY| |REPLAY PROTECTION COUNTER| |OUT-OF-ORDER REPLAY WINDOW|
The encryption and authentication keys will generally be specified using
syntax such as C<\E<lt>0183 A947 1ABE 01FF FA04 103B B102<gt>>.
 This module uses 4 and 5 annotation space integers to pass Security Association Data between IPsec modules.

=a RadixIPLookup, RangeIPsecLookup */


//Hosts IPsec extensions
struct IPsecRoute {
    IPAddress addr;
    IPAddress mask;
    IPAddress gw;
    int32_t port;
    int32_t extra;
    /*IPsec extensions*/
    uint32_t spi;
    SADataTuple * sa_data;

    IPsecRoute()			: port(-1) { }

    inline bool real() const	{ return port > (int32_t) -0x80000000; }
    inline void kill()		{ addr = 0; mask = 0xFFFFFFFFU; port = -0x80000000; }
    inline bool contains(IPAddress a) const;
    inline bool contains(const IPsecRoute& r) const;
    inline bool mask_as_specific(IPAddress m) const;
    inline bool mask_as_specific(const IPsecRoute& r) const;
    inline bool match(const IPsecRoute& r) const;
    int prefix_len() const	{ return mask.mask_to_prefix_len(); }

    StringAccum &unparse(StringAccum&, bool tabs) const;
    String unparse() const;
    String unparse_addr() const	{ return addr.unparse_with_mask(mask); }
};


class IPsecRouteTable : public Element { public:

    void* cast(const char*);
    int configure(Vector<String>&, ErrorHandler*) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    virtual int add_route(const IPsecRoute& route, bool allow_replace, IPsecRoute* replaced_route, ErrorHandler* errh);
    virtual int remove_route(const IPsecRoute& route, IPsecRoute* removed_route, ErrorHandler* errh);
    virtual int lookup_route(IPAddress dest, IPAddress &gw, unsigned int &spi, SADataTuple * &sa_data) const = 0;
    virtual String dump_routes();

    void push(int port, Packet* p);

    static int add_route_handler(const String&, Element*, void*, ErrorHandler*);
    static int remove_route_handler(const String&, Element*, void*, ErrorHandler*);
    static int ctrl_handler(const String&, Element*, void*, ErrorHandler*);
    static int lookup_handler(int operation, String&, Element*, const Handler*, ErrorHandler*);
    static String table_handler(Element*, void*);
    /*IPSEC extension: The security association database entry*/
    SATable _sa_table;

  private:
    enum { CMD_ADD, CMD_SET, CMD_REMOVE };
    int run_command(int command, const String &, Vector<IPsecRoute>* old_routes, ErrorHandler*);

};

inline StringAccum&
operator<<(StringAccum& sa, const IPsecRoute& route)
{
    return route.unparse(sa, false);
}

inline bool
IPsecRoute::contains(IPAddress a) const
{
    return a.matches_prefix(addr, mask);
}

inline bool
IPsecRoute::contains(const IPsecRoute& r) const
{
    return r.addr.matches_prefix(addr, mask) && r.mask.mask_as_specific(mask);
}

inline bool
IPsecRoute::mask_as_specific(IPAddress m) const
{
    return mask.mask_as_specific(m);
}

inline bool
IPsecRoute::mask_as_specific(const IPsecRoute& r) const
{
    return mask.mask_as_specific(r.mask);
}

inline bool
IPsecRoute::match(const IPsecRoute& r) const
{
    return addr == r.addr && mask == r.mask
	&& (port < 0 || (gw == r.gw && port == r.port));
}

CLICK_ENDDECLS
#endif
