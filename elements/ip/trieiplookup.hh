// -*- c-basic-offset: 4 -*-
#ifndef CLICK_TRIEIPLOOKUP_HH
#define CLICK_TRIEIPLOOKUP_HH
#include "iproutetable.hh"
#include <click/hashmap.hh>
CLICK_DECLS

/*
=c
TrieIPLookup()

=s IP, classification
IP lookup using an array of hashmaps

=d
Performs IP lookup using an array of hashmaps in logarithmic time,
using the IPRouteTable interface, at the cost of approximately n log n
initialization time.  See IPRouteTable for description.

Warning: This element is experimental.

=a IPRouteTable
*/

class TrieIPLookup : public IPRouteTable {
public:
    
    // constructors
    TrieIPLookup();
    ~TrieIPLookup();
    
    const char *class_name() const  { return "TrieIPLookup"; }
    const char *processing() const  { return PUSH; }
    TrieIPLookup *clone()    const  { return new TrieIPLookup; }
    
    void notify_noutputs(int);
    int initialize(ErrorHandler *);
    void add_handlers();
    
    int configure(Vector<String> &, ErrorHandler *);
    
    int add_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress &) const;
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
        operator int() const { return addr; }
        String unparse() const
        {
            const unsigned char *p = (unsigned char *)(&addr);
            char buf[20];
            sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
            return String(buf);
        }
    };

    struct Prefix {
        IPAddress addr, mask, gw;
        int output;

        Prefix(const IPAddress& a, const IPAddress& m, const IPAddress& g, int o) :
            addr(a), mask(m), gw(g), output(o) {}

        String unparse() const { return addr.unparse_with_mask(mask) +
                                     " " + gw.unparse() + " " + String(output); }
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
        IPAddress addr, mask, gw;
        int output;

        long left_child;
        long right_child;
        long parent;

        uint32_t children_lengths;
        bool is_real;

        long index;
	
        String unparse() const
        {
            String str;
            str += addr.unparse();
            str += mask.unparse();
            str += gw.unparse();
            str += String(output) + String(" ");
            str += String("left_child: ") + String(left_child) + String(" ");
            str += String("right_child: ") + String(right_child) + String(" ");
            str += String(children_lengths);
            str += String(is_real);
            return str;
        }

        TrieNode() :
            addr(0), mask(0), gw(0), output(0), left_child(-1), right_child(-1),
            parent(-1), children_lengths(0), is_real(false), index(-1) {}

        TrieNode(const IPAddress& a, const IPAddress& m, const IPAddress& g,
                 int o, bool real, int i = -1) :
            addr(a), mask(m), gw(g), output(o), is_real(real), index(i)
        {
            left_child = -1;
            right_child = -1;
            parent = -1;
            children_lengths = 0;
        }

        inline bool
        operator<(const TrieNode& tn) const
        {
            return (htonl(addr.addr()) < htonl(tn.addr.addr())) ||
                (addr.addr() == tn.addr.addr() &&
                 mask.mask_to_prefix_len() < tn.mask.mask_to_prefix_len());
        }

    };

protected:
    // helper methods
    inline int  add_to_route_vector(IPAddress addr, IPAddress mask, IPAddress gw, int output, ErrorHandler *errh);
    void build_main();
    inline void build_init();
    inline void build_trie();

    // return the length of the mask of the middle, 0 if no middle
    inline int  build_exists_middle(const TrieNode& parent, const TrieNode& child);
    inline void build_middle(int prefix_length, const TrieNode& parent, const TrieNode& child);
    inline void build_children_lengths(int);
    inline void build_hash(int);
    inline void build_hash_node(TrieNode& tn, const Rope& rope, int upper_bound_inclusive);
    inline void build_hash_marker(Marker &new_marker, TrieNode tn, int n_prefix_length,
                                  int n_array_index, int upper_bound_inclusive);


    // print methods for debugging
    void print_route_vector() const;
    void print_lengthhash(const LengthHash& lengthhash) const;
    void print_trie(const TrieNode& tn) const;
    
    // check methods
    void check_init();
    void check_trie_node(const TrieNode&);
    void check_trie(const TrieNode&);
    void check_lengths(const TrieNode&);

    // member variables
    Rope _default_rope;
    LengthHash _lengths_array[33];     // array containing a hashmap for each length + 0
    Vector<Prefix> _route_vector;      // vector of all routes we know about

    Vector<TrieNode> trie_vector;      // used only during build
};

CLICK_ENDDECLS
#endif
