// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RADIXIPLOOKUP_HH
#define CLICK_RADIXIPLOOKUP_HH
#include <click/glue.hh>
#include <click/element.hh>
#include "iproutetable.hh"
CLICK_DECLS
class Radix;

/*
 * =c
 * RadixIPLookup()
 * =s IP, classification
 * IP lookup using a radix table
 * =d
 *
 * Performs IP lookup using a radix table, using the IPRouteTable
 * interface. See IPRouteTable for description.
 *
 * Warning: This element does not work.
 *
 * =a IPRouteTable
 */

class RadixIPLookup : public IPRouteTable { public:
    
    RadixIPLookup();
    ~RadixIPLookup();

    const char *class_name() const		{ return "RadixIPLookup"; }
    const char *processing() const		{ return PUSH; }
    RadixIPLookup *clone() const		{ return new RadixIPLookup; }

    void notify_noutputs(int);
    void cleanup(CleanupStage);

    int add_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *);
    int lookup_route(IPAddress, IPAddress &) const;
    String dump_routes() const;

  private:
    
    bool get(int i, unsigned &dst, unsigned &mask, unsigned &gw, unsigned &port) const;

    // Simple routing table
    struct Entry {
	unsigned dst;
	unsigned mask;
	unsigned gw;
	unsigned port;
	bool valid;
    };
    Vector<Entry *> _v;
    int _entries;
    Radix *_radix;
    
};

class Radix { public:
    // implementation take from Sedgewick

    Radix();
    ~Radix();

    void insert(unsigned v, int info);
    void del(unsigned v);
    bool lookup(unsigned v, int &info);

  private:
    
    struct RadixNode {
	unsigned key;
	int bit_idx;
	RadixNode *left;
	RadixNode *right;

	bool valid;
	int info;
	RadixNode() : left(0), right(0), valid(false) { }
	void kill();
    };

    struct RadixNode *root;
    static unsigned bits(unsigned x, unsigned y, unsigned char idx);
    RadixNode *node_lookup(unsigned v);
    enum { KEYSIZE = 32 };
    
};


inline
Radix::Radix()
{
    root = new RadixNode;
    root->info = root->key = 0;
    root->bit_idx = KEYSIZE-1;
    root->valid = false;
    root->left = root->right = root;
}

inline void
Radix::RadixNode::kill()
{
    if (bit_idx >= 0) {
	bit_idx = -1;
	if (left) left->kill();
	if (right) right->kill();
	delete this;
    }
}

inline
Radix::~Radix()
{
    root->kill();
}

inline void
Radix::insert(unsigned v, int info)
{
    RadixNode *t, *p;
    int i;

    t = node_lookup(v);

    if (v == t->key)
	return;

    // Find bit for which v and t-> key differ
    i = KEYSIZE-1;
    while (bits(v, i, 1) == bits(t->key, i, 1))
	i--;

    // Descend to that level
    RadixNode *x = root;
    do {
	p = x;
	if(bits(v, x->bit_idx, 1) == 0)
	    x = x->left;
	else
	    x = x->right;

	if((x->bit_idx <= i) || (p->bit_idx <= x->bit_idx))
	    break;
    } while(1);

    // ... and instert node
    t = new RadixNode;
    t->key = v;
    t->bit_idx = i;
    t->info = info;
    t->valid = true;

    if (bits(v, t->bit_idx, 1) == 0) {
	t->left = t;
	t->right = x;
    } else {
	t->right = t;
	t->left = x;
    }

    if (bits(v, p->bit_idx, 1) == 0)
	p->left = t;
    else
	p->right = t;
}


// returns info based on key
inline bool
Radix::lookup(unsigned v, int &info)
{
    RadixNode *t;

    t = node_lookup(v);
    if (t->valid) {
	info = t->info;
	return true;
    } else
	return false;
}

inline void
Radix::del(unsigned v)
{
    RadixNode *t;

    t = node_lookup(v);

    // only delete if this is an exact match
    if (t->key == v)
	t->valid = false;
}


// returns node based on key
inline Radix::RadixNode *
Radix::node_lookup(unsigned v)
{
    RadixNode *p, *x = root;

    do {
	p = x;
	if (bits(v, x->bit_idx, 1) == 0)
	    x = x->left;
	else
	    x = x->right;
    } while (x->bit_idx < p->bit_idx);

    return x;
}

// returns j bytes which appear k from rhe right. Rightmost bit has index 0.
inline unsigned
Radix::bits(unsigned x, unsigned k, unsigned char j)
{
    return (x >> k) & (0xffffffff >> (KEYSIZE-j));
}

CLICK_ENDDECLS
#endif
