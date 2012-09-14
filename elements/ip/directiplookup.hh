// -*- c-basic-offset: 4 -*-
#ifndef CLICK_DIRECTIPLOOKUP_HH
#define CLICK_DIRECTIPLOOKUP_HH
#include "iproutetable.hh"
CLICK_DECLS

/*
=c

DirectIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s iproute

IP routing lookup using direct-indexed tables

=d

Expects a destination IP address annotation with each packet. Looks up that
address in its routing table, using longest-prefix-match, sets the destination
annotation to the corresponding GW (if specified), and emits the packet on the
indicated OUTput port.

Each argument is a route, specifying a destination and mask, an optional
gateway IP address, and an output port.  No destination-mask pair should occur
more than once.

DirectIPLookup is optimized for lookup speed at the expense of extensive RAM
usage. Each longest-prefix lookup is accomplished in one to maximum two DRAM
accesses, regardless on the number of routing table entries. Individual
entries can be dynamically added to or removed from the routing table with
relatively low CPU overhead, allowing for high update rates.

DirectIPLookup implements the I<DIR-24-8-BASIC> lookup scheme described by
Gupta, Lin, and McKeown in the paper cited below.

=h table read-only

Outputs a human-readable version of the current routing table.

=h lookup read-only, requires parameters

Reports the OUTput port and GW corresponding to an address.

=h add write-only

Adds a route to the table. Format should be `C<ADDR/MASK [GW] OUT>'.
Fails if a route for C<ADDR/MASK> already exists.

=h set write-only

Sets a route, whether or not a route for the same prefix already exists.

=h remove write-only

Removes a route from the table. Format should be `C<ADDR/MASK>'.

=h ctrl write-only

Adds or removes a group of routes. Write `C<add>/C<set ADDR/MASK [GW] OUT>' to
add a route, and `C<remove ADDR/MASK>' to remove a route. You can supply
multiple commands, one per line; all commands are executed as one atomic
operation.

=h flush write-only

Clears the entire routing table in a single atomic operation.

=n

See IPRouteTable for a performance comparison of the various IP routing
elements.

DirectIPLookup's data structures are inherently limited: at most 2^16 /24
networks can contain routes for /25-or-smaller subnetworks, no matter how much
memory you have.  If you need more than this, try RangeIPLookup.

=a IPRouteTable, RangeIPLookup, RadixIPLookup, StaticIPLookup, LinearIPLookup,
SortedIPLookup, LinuxIPLookup

Pankaj Gupta, Steven Lin, and Nick McKeown.  "Routing Lookups in Hardware at
Memory Access Speeds".  In Proc. IEEE Infocom 1998, Vol. 3, pp. 1240-1247.

*/

class RangeIPLookup;

class DirectIPLookup : public IPRouteTable { public:

    DirectIPLookup() CLICK_COLD;
    ~DirectIPLookup() CLICK_COLD;

    const char *class_name() const	{ return "DirectIPLookup"; }
    const char *port_count() const	{ return "1/-"; }
    const char *processing() const	{ return PUSH; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage stage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void push(int port, Packet* p);

    int add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *);
    int remove_route(const IPRoute&, IPRoute*, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress&) const;
    String dump_routes();

    static int flush_handler(const String &, Element *, void *, ErrorHandler *);

    enum {
	RT_SIZE_MAX = 256 * 1024, // accomodate a full BGP view and more
	tbl_24_31_capacity_limit = 32768 * 256,
	vport_capacity_limit = 32768,
	PREF_HASHSIZE = 64 * 1024, // must be a power of 2!
	DISCARD_PORT = -1
    };

    struct CleartextEntry {
	int ll_next;
	int ll_prev;
	uint32_t prefix;
	uint16_t plen;
	int16_t vport;
    };

    struct VirtualPort {
	int16_t ll_next;
	int16_t ll_prev;
	int32_t refcount;
	IPAddress gw;
	int16_t port;
	int16_t padding;
    };

    struct Table {
	// Structures used for IP lookup
	uint16_t *_tbl_0_23;
	uint16_t *_tbl_24_31;
	VirtualPort *_vport;

	// Structures used for lookup table maintenance (add/remove operations)
	CleartextEntry *_rtable;
	int *_rt_hashtbl; // [PREF_HASHSIZE];
	uint8_t *_tbl_0_23_plen;
	uint8_t *_tbl_24_31_plen;

	uint32_t _rtable_size;
	uint32_t _tbl_24_31_size;
	uint32_t _vport_size;
	int _rt_empty_head;
	uint16_t _tbl_24_31_empty_head;
	int _vport_head;
	int _vport_empty_head;

	uint32_t _rtable_capacity;
	uint32_t _tbl_24_31_capacity;
	uint32_t _vport_capacity;

	Table()
	    : _tbl_0_23(0), _tbl_24_31(0), _vport(0), _rtable(0),
	      _rt_hashtbl(0), _tbl_0_23_plen(0), _tbl_24_31_plen(0) {
	}

	~Table() {
	    cleanup();
	}

	int initialize();
	void cleanup();

	static inline uint32_t prefix_hash(uint32_t, uint32_t);

	int find_entry(uint32_t, uint32_t) const;
	String dump() const;

	int vport_find(IPAddress gw, int16_t port);
	void vport_unref(uint16_t);

	int add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *);
	int remove_route(const IPRoute&, IPRoute*, ErrorHandler *);
	void flush();

    };

  protected:

    Table _t;

    friend class RangeIPLookup;

};

CLICK_ENDDECLS
#endif
