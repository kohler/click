// -*- c-basic-offset: 4 -*-
/*
 * anonipaddr.{cc,hh} -- anonymize packet IP addresses
 * Eddie Kohler after Greg Minshall
 *
 * Copyright (c) 2001 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "anonipaddr.hh"
#include <click/standard/scheduleinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/llrpc.h>
#include <click/integers.hh>	// for first_bit_set
#ifdef CLICK_USERLEVEL
# include <unistd.h>
# include <time.h>
# include <sys/time.h>
#endif
CLICK_DECLS

AnonymizeIPAddr::AnonymizeIPAddr()
    : Element(1, 0), _root(0), _free(0)
{
    MOD_INC_USE_COUNT;
}

AnonymizeIPAddr::~AnonymizeIPAddr()
{
    MOD_DEC_USE_COUNT;
}

AnonymizeIPAddr::Node *
AnonymizeIPAddr::new_node_block()
{
    assert(!_free);
    int block_size = 1024;
    Node *block = new Node[block_size];
    if (!block)
	return 0;
    _blocks.push_back(block);
    for (int i = 1; i < block_size - 1; i++)
	block[i].child[0] = &block[i+1];
    block[block_size - 1].child[0] = 0;
    _free = &block[1];
    return &block[0];
}

static uint32_t
rand32()
{
    return ((random()&0xffff)<<16)|(random()&0xffff);
}

void
AnonymizeIPAddr::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
}

int
AnonymizeIPAddr::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _preserve_class = 0;
    String preserve_8;
    bool seed = true;
    
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "CLASS", cpInteger, "preserve class bits?", &_preserve_class,
		    "PRESERVE_8", cpArgument, "list of /8s to preserve", &preserve_8,
		    "SEED", cpBool, "seed random number generator?", &seed,
		    0) < 0)
	return -1;

    // check CLASS value
    if (_preserve_class == 99)	// allow 99 as synonym for 32
	_preserve_class = 32;
    else if (_preserve_class > 32)
	errh->error("CLASS must be between 0 and 32");

    // check preserve_8
    if (preserve_8) {
	Vector<String> words;
	int what;
	cp_spacevec(preserve_8, words);
	for (int i = 0; i < words.size(); i++)
	    if (cp_integer(words[i], &what) && what >= 0 && what < 256)
		_preserve_8.push_back(what);
	    else
		return errh->error("bad PRESERVE_8 argument `%s', should be integer between 0 and 255", words[i].cc());
    }

    // install seed if required
    if (seed)
	click_random_srandom();
    
    return 0;
}

int
AnonymizeIPAddr::initialize(ErrorHandler *errh)
{
    if (!(_root = new_node()))
	return errh->error("out of memory!");
    _root->input = 1;		// use 1 instead of 0 b/c 0.0.0.0 is special
    _root->output = rand32();
    _root->child[0] = _root->child[1] = 0;
    bool root_touched = false;

    // preserve classes
    if (_preserve_class > 0) {
	assert((0xFFFFFFFFU >> 1) == 0x7FFFFFFFU);
	uint32_t class_mask = ~(0xFFFFFFFFU >> _preserve_class);
	_root->input = class_mask;
	_root->output |= class_mask;
	root_touched = true;
    }

    // preserve requested /8s
    for (int i = 0; i < _preserve_8.size(); i++) {
	uint32_t addr = (_preserve_8[i] << 24);
	if (!root_touched) {
	    _root->input = addr;
	    _root->output = (_root->output & 0x00FFFFFF) | addr;
	    root_touched = true;
	} else if (Node *n = find_node(addr))
	    n->output = (n->output & 0x00FFFFFF) | addr;
	else
	    return errh->error("out of memory!");
    }

    // prepare special nodes for 0.0.0.0 and 255.255.255.255
    memset(&_special_nodes[0], 0, sizeof(_special_nodes));
    _special_nodes[0].input = _special_nodes[0].output = 0;
    _special_nodes[1].input = _special_nodes[1].output = 0xFFFFFFFF;
    
    return 0;
}

void
AnonymizeIPAddr::cleanup(CleanupStage)
{
    for (int i = 0; i < _blocks.size(); i++)
	delete[] _blocks[i];
    _blocks.clear();
}

uint32_t
AnonymizeIPAddr::make_output(uint32_t old_output, int swivel) const
{
    // -A50 anonymization
    if (swivel == 32)
	return old_output ^ 1;
    else {
	// bits up to swivel are unchanged; bit swivel is flipped
	uint32_t known_part =
	    ((old_output >> (32 - swivel)) ^ 1) << (32 - swivel);
	// rest of bits are random
	return known_part | ((rand32() & 0x7FFFFFFF) >> swivel);
    }
}

AnonymizeIPAddr::Node *
AnonymizeIPAddr::make_peer(uint32_t a, Node *n)
{
    // watch out for special IP addresses, which never make it into the tree
    if (a == 0 || a == 0xFFFFFFFFU)
	return &_special_nodes[a & 1];
    
    /*
     * become a peer
     * algo: create two nodes, the two peers.  leave orig node as
     * the parent of the two new ones.
     */

    Node *down[2];
    if (!(down[0] = new_node()))
	return 0;
    if (!(down[1] = new_node())) {
	free_node(down[0]);
	return 0;
    }

    // swivel is first bit 'a' and 'old->input' differ
    int swivel = first_bit_set(a ^ n->input);
    // bitvalue is the value of that bit of 'a'
    int bitvalue = (a >> (32 - swivel)) & 1;

    down[bitvalue]->input = a;
    down[bitvalue]->output = make_output(n->output, swivel);
    down[bitvalue]->child[0] = down[bitvalue]->child[1] = 0;

    *down[1 - bitvalue] = *n;	/* copy orig node down one level */

    n->input = down[1]->input;	/* NB: 1s to the right (0s to the left) */
    n->output = down[1]->output;
    n->child[0] = down[0];	/* point to children */
    n->child[1] = down[1];

    return down[bitvalue];
}

AnonymizeIPAddr::Node *
AnonymizeIPAddr::find_node(uint32_t a)
{
    // straight outta tcpdpriv
    Node *n = _root;
    while (n) {
	if (n->input == a)
	    return n;
	if (!n->child[0])
	    n = make_peer(a, n);
	else {
	    // swivel is the first bit in which the two children differ
	    int swivel = first_bit_set(n->child[0]->input ^ n->child[1]->input);
	    if (first_bit_set(a ^ n->input) < swivel) // input differs earlier
		n = make_peer(a, n);
	    else if (a & (1 << (32 - swivel)))
		n = n->child[1];
	    else
		n = n->child[0];
	}
    }
    
    click_chatter("AnonymizeIPAddr: out of memory!");
    return 0;
}

inline uint32_t
AnonymizeIPAddr::anonymize_addr(uint32_t a)
{
    if (Node *n = find_node(ntohl(a)))
	return htonl(n->output);
    else
	return 0;
}

Packet *
AnonymizeIPAddr::simple_action(Packet *p)
{
    const click_ip *in_iph = p->ip_header();
    if (!in_iph || in_iph->ip_v != 4) {
	checked_output_push(1, p);
	return 0;
    } else if (WritablePacket *q = p->uniqueify()) {
	click_ip *iph = q->ip_header();
	uint32_t src = iph->ip_src.s_addr, dst = iph->ip_dst.s_addr;
	
	// incrementally update IP checksum according to RFC1624:
	// new_sum = ~(~old_sum + ~old_halfword + new_halfword)
	uint32_t sum = (~iph->ip_sum & 0xFFFF)
	    + (~src & 0xFFFF) + (~src >> 16) + (~dst & 0xFFFF) + (~dst >> 16);
	
	iph->ip_src.s_addr = src = anonymize_addr(src);
	iph->ip_dst.s_addr = dst = anonymize_addr(dst);

	sum += (src & 0xFFFF) + (src >> 16) + (dst & 0xFFFF) + (dst >> 16);
	sum = (sum & 0xFFFF) + (sum >> 16);
	iph->ip_sum = ~(sum + (sum >> 16));
	
	return q;
    } else
	return 0;
}

int
AnonymizeIPAddr::llrpc(unsigned command, void *data)
{
    if (command == CLICK_LLRPC_MAP_IPADDRESS) {
	// XXX should lock handler
	uint32_t *val = reinterpret_cast<uint32_t *>(data);
	*val = anonymize_addr(*val);
	return 0;

    } else
	return Element::llrpc(command, data);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AnonymizeIPAddr)
