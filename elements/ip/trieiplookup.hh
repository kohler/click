// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TRIEIPLOOKUP_HH
#define CLICK_TRIEIPLOOKUP_HH
#include "iproutetable.hh"
#include <click/hashmap.hh>
CLICK_DECLS

/*
=c

TrieIPLookup(ADDR1/MASK1 [GW1] OUT1, ADDR2/MASK2 [GW2] OUT2, ...)

=s IP, classification

IP lookup using an array of hashmaps

=d

Performs IP lookup using an array of hashmaps in logarithmic time, at the cost
of approximately n log n initialization time (and a large constant factor).

Expects a destination IP address annotation with each packet. Looks up that
address in its routing table, using longest-prefix-match, sets the destination
annotation to the corresponding GW (if specified), and emits the packet on the
indicated OUTput port.

Each argument is a route, specifying a destination and mask, an optional
gateway IP address, and an output port.

Uses the IPRouteTable interface; see IPRouteTable for description.

Warning: This element is experimental.

=h table read-only

Outputs a human-readable version of the current routing table.

=h lookup read-only

Reports the OUTput port and GW corresponding to an address.

=h add write-only

Adds a route to the table. Format should be `C<ADDR/MASK [GW] OUT>'. Should
fail if a route for C<ADDR/MASK> already exists, but currently does not.

=h set write-only

Sets a route, whether or not a route for the same prefix already exists.

=h remove write-only

Removes a route from the table. Format should be `C<ADDR/MASK>'.

=h ctrl write-only

Adds or removes a group of routes. Write `C<add>/C<set ADDR/MASK [GW] OUT>' to
add a route, and `C<remove ADDR/MASK>' to remove a route. You can supply
multiple commands, one per line; all commands are executed as one atomic
operation.

=a IPRouteTable, StaticIPLookup, LinearIPLookup, SortedIPLookup,
DirectIPLookup, TrieIPLookup
*/

class TrieIPLookup : public IPRouteTable {
public:
    
    // constructors
    TrieIPLookup();
    ~TrieIPLookup();
    
    const char *class_name() const  { return "TrieIPLookup"; }
    const char *processing() const  { return PUSH; }
    
    void notify_noutputs(int);
    int configure(Vector<String> &, ErrorHandler *);
        
    int add_route(const IPRoute&, bool, IPRoute*, ErrorHandler *);
    int remove_route(const IPRoute&, IPRoute*, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress&) const;
    String dump_routes() const;

    // data structures
    typedef Vector<int> Rope;
    
    struct HashIPAddress {
        uint32_t addr;
        bool is_real;

        HashIPAddress() : addr(0), is_real(false) { }
        HashIPAddress(uint32_t a) : addr(a), is_real(true) { }
        HashIPAddress(IPAddress a) : addr(a), is_real(true) { }
        bool operator==(const HashIPAddress& a)
        { return (a.addr == addr) && (a.is_real == is_real); }
        operator bool() const { return is_real; }
        operator unsigned() const { return addr; }
        String unparse() const
        {
            const unsigned char *p = (unsigned char *)(&addr);
            char buf[20];
            sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
            return String(buf);
        }
    };

    struct Marker {
        Rope rope;
        IPAddress gw;
        int output;

        String unparse() const
        {
            String str;
            str += "rope: ";
            for (int i = rope.size() - 1; i >= 0; i--) {
                str += (String(rope[i]) + " ");
            }
            str += ("gw: " + String(gw.addr()));
            str += (" out: " + String(output));

            return str;
        }
    };
    
    typedef HashMap<HashIPAddress, Marker> LengthHash;

    struct TrieNode {
	IPRoute r;

        long left_child;
        long right_child;
        long parent;

        uint32_t children_lengths;
        bool is_real;

        long index;
	
        String unparse() const
        {
            String str;
            str += r.unparse() + String(" ");
            str += String("left_child: ") + String(left_child) + String(" ");
            str += String("right_child: ") + String(right_child) + String(" ");
            str += String(children_lengths);
            str += String(is_real);
            return str;
        }

        TrieNode() :
            left_child(-1), right_child(-1),
            parent(-1), children_lengths(0), is_real(false), index(-1) {}

        TrieNode(const IPRoute& r, bool real, int i = -1) :
            r(r), is_real(real), index(i)
        {
            left_child = -1;
            right_child = -1;
            parent = -1;
            children_lengths = 0;
        }

        inline bool
        operator<(const TrieNode& tn) const
        {
            return (htonl(r.addr.addr()) < htonl(tn.r.addr.addr())) ||
                (r.addr == tn.r.addr &&
                 r.mask.mask_to_prefix_len() < tn.r.mask.mask_to_prefix_len());
        }

    };

protected:
    // helper methods
    inline int binary_search(const Vector<IPRoute> &vec, const IPRoute &pf);

     // build methods
    void build_main();
    inline void build_init();
    inline void build_trie();
    // return the length of the mask of the middle, 0 if no middle
    inline int  build_exists_middle(const TrieNode& parent, const TrieNode& child);
    inline void build_middle(int prefix_length, const TrieNode& parent, const TrieNode& child);
    void build_children_lengths(int);
    void build_hash(int);
    void build_hash_node(TrieNode& tn, const Rope& rope, int upper_bound_inclusive);
    inline void build_hash_marker(Marker &new_marker, TrieNode tn, int n_prefix_length,
                                  int n_array_index, int upper_bound_inclusive);

    // print methods for debugging
    void print_route_vector() const;
    void print_lengthhash(const LengthHash& lengthhash) const;
    void print_trie(const TrieNode& tn) const;
    
    // check methods
    void check_route_vector_sorted();
    void check_init();
    void check_trie_node(const TrieNode&);
    void check_trie(const TrieNode&);
    void check_lengths(const TrieNode&);

    // member variables
    Rope _default_rope;
    LengthHash _lengths_array[33];     // array containing a hashmap for each length + 0
    Vector<IPRoute> _route_vector;     // vector of all routes we know about
                                       // must be sorted and no duplicates
    bool _active;		       // true once trie is active
    Vector<TrieNode> _trie_vector;     // used only during build
    
};

CLICK_ENDDECLS
#endif
