/*
 * aggcounter.{cc,hh} -- count packets/bytes with given aggregate
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
#include "aggcounter.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <packet_anno.hh>

AggregateCounter::AggregateCounter()
    : Element(1, 1), _root(0), _free(0)
{
    MOD_INC_USE_COUNT;
}

AggregateCounter::~AggregateCounter()
{
    MOD_DEC_USE_COUNT;
}

AggregateCounter::Node *
AggregateCounter::new_node_block()
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

void
AggregateCounter::notify_ninputs(int n)
{
    set_ninputs(n <= 1 ? 1 : 2);
}

void
AggregateCounter::notify_noutputs(int n)
{
    set_noutputs(n <= 1 || ninputs() == 1 ? 1 : 2);
}

int
AggregateCounter::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    bool bytes = false;
    bool packet_count = true;
    bool extra_length = true;
    _freeze_nnz = 0xFFFFFFFFU;
    _freeze_count = (uint64_t)(-1);
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "BYTES", cpBool, "count bytes?", &bytes,
		    "MULTIPACKET", cpBool, "use packet count annotation?", &packet_count,
		    "EXTRA_LENGTH", cpBool, "use extra length annotation?", &extra_length,
		    "FREEZE_AFTER_AGG", cpUnsigned, "freeze after N nonzero aggregates", &_freeze_nnz,
		    "FREEZE_AFTER_COUNT", cpUnsigned64, "freeze after count reaches N", &_freeze_count,
		    0) < 0)
	return -1;
    _bytes = bytes;
    _packet_count = packet_count;
    _extra_length = extra_length;
    _frozen = false;
    return 0;
}

int
AggregateCounter::initialize(ErrorHandler *errh)
{
    if (!(_root = new_node())) {
	uninitialize();
	return errh->error("out of memory!");
    }
    _root->aggregate = 0;
    _root->count = 0;
    _root->child[0] = _root->child[1] = 0;
    _num_nonzero = 0;
    _count = 0;
    
    return 0;
}

void
AggregateCounter::uninitialize()
{
    for (int i = 0; i < _blocks.size(); i++)
	delete[] _blocks[i];
    _blocks.clear();
}

// from tcpdpriv
int
bi_ffs(uint32_t value)
{
    int add = 0;
    static uint8_t bvals[] = { 0, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 };

    if ((value & 0xFFFF0000) == 0) {
	if (value == 0) {	/* zero input ==> zero output */
	    return 0;
	}
	add += 16;
    } else {
	value >>= 16;
    }
    if ((value & 0xFF00) == 0) {
	add += 8;
    } else {
	value >>= 8;
    }
    if ((value & 0xF0) == 0) {
	add += 4;
    } else {
	value >>= 4;
    }
    return add + bvals[value & 0xf];
}

AggregateCounter::Node *
AggregateCounter::make_peer(uint32_t a, Node *n, bool frozen)
{
    /*
     * become a peer
     * algo: create two nodes, the two peers.  leave orig node as
     * the parent of the two new ones.
     */

    if (frozen)
	return 0;
    
    Node *down[2];
    if (!(down[0] = new_node()))
	return 0;
    if (!(down[1] = new_node())) {
	free_node(down[0]);
	return 0;
    }

    // swivel is first bit 'a' and 'old->input' differ
    int swivel = bi_ffs(a ^ n->aggregate);
    // bitvalue is the value of that bit of 'a'
    int bitvalue = (a >> (32 - swivel)) & 1;

    down[bitvalue]->aggregate = a;
    down[bitvalue]->count = 0;
    down[bitvalue]->child[0] = down[bitvalue]->child[1] = 0;

    *down[1 - bitvalue] = *n;	/* copy orig node down one level */

    n->aggregate = down[1]->aggregate; /* NB: 1s to the right (0s to the left) */
    n->count = down[1]->count;
    n->child[0] = down[0];	/* point to children */
    n->child[1] = down[1];

    down[1]->count = 0;

    return (n->aggregate == a ? n : down[bitvalue]);
}

AggregateCounter::Node *
AggregateCounter::find_node(uint32_t a, bool frozen)
{
    // straight outta tcpdpriv
    Node *n = _root;
    while (n) {
	if (n->aggregate == a)
	    return (n->count || !frozen ? n : 0);
	if (!n->child[0])
	    n = make_peer(a, n, frozen);
	else {
	    // swivel is the first bit in which the two children differ
	    int swivel = bi_ffs(n->child[0]->aggregate ^ n->child[1]->aggregate);
	    if (bi_ffs(a ^ n->aggregate) < swivel) // input differs earlier
		n = make_peer(a, n, frozen);
	    else if (a & (1 << (32 - swivel)))
		n = n->child[1];
	    else
		n = n->child[0];
	}
    }

    if (!frozen)
	click_chatter("AggregateCounter: out of memory!");
    return 0;
}

inline void
AggregateCounter::update(Packet *p, bool frozen)
{
    // AGGREGATE_ANNO is already in host byte order!
    uint32_t agg = AGGREGATE_ANNO(p);
    if (Node *n = find_node(agg, frozen)) {
	uint32_t amount;
	if (!_bytes)
	    amount = (_packet_count && PACKET_COUNT_ANNO(p) ? PACKET_COUNT_ANNO(p) : 1);
	else
	    amount = p->length() + (_extra_length ? EXTRA_LENGTH_ANNO(p) : 0);
	if (amount && !n->count)
	    _num_nonzero++;
	n->count += amount;
	_count += amount;
	if (_num_nonzero >= _freeze_nnz || _count >= _freeze_count)
	    _frozen = true;
    }
}

void
AggregateCounter::push(int port, Packet *p)
{
    update(p, _frozen || (port == 1));
    if (port == 0)
	output(0).push(p);
    else
	output(noutputs() - 1).push(p);
}

Packet *
AggregateCounter::pull(int port)
{
    Packet *p = input(port).pull();
    if (p)
	update(p, _frozen || (port == 1));
    return p;
}

static void
write_batch(FILE *f, bool binary, uint32_t *buffer, int pos,
	    ErrorHandler *)
{
    if (binary)
	fwrite(buffer, sizeof(uint32_t), pos, f);
    else
	for (int i = 0; i < pos; i += 2)
	    fprintf(f, "%u %u\n", buffer[i], buffer[i+1]);
}

void
AggregateCounter::write_nodes(Node *n, FILE *f, bool binary,
				 uint32_t *buffer, int &pos, int len,
				 ErrorHandler *errh)
{
    if (n->count > 0) {
	buffer[pos++] = n->aggregate;
	buffer[pos++] = n->count;
	if (pos == len) {
	    write_batch(f, binary, buffer, pos, errh);
	    pos = 0;
	}
    }

    if (n->child[0])
	write_nodes(n->child[0], f, binary, buffer, pos, len, errh);
    if (n->child[1])
	write_nodes(n->child[1], f, binary, buffer, pos, len, errh);
}

int
AggregateCounter::write_file(String where, bool binary,
				ErrorHandler *errh) const
{
    FILE *f;
    if (where == "-")
	f = stdout;
    else
	f = fopen(where.cc(), (binary ? "wb" : "w"));
    if (!f)
	return errh->error("%s: %s", where.cc(), strerror(errno));
    
    fprintf(f, "$num_nonzero %u\n", _num_nonzero);
#if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    if (binary)
	fprintf(f, "$packed_be\n");
#elif CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    if (binary)
	fprintf(f, "$packed_le\n");
#else
    binary = false;
#endif
    
    uint32_t buf[1024];
    int pos = 0;
    write_nodes(_root, f, binary, buf, pos, 1024, errh);
    if (pos)
	write_batch(f, binary, buf, pos, errh);

    bool had_err = ferror(f);
    if (f != stdout)
	fclose(f);
    if (had_err)
	return errh->error("%s: file error", where.cc());
    else
	return 0;
}

int
AggregateCounter::write_file_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh)
{
    AggregateCounter *ac = static_cast<AggregateCounter *>(e);
    String fn;
    if (!cp_filename(cp_uncomment(data), &fn))
	return errh->error("argument should be filename");
    return ac->write_file(fn, (thunk != 0), errh);
}

String
AggregateCounter::read_handler(Element *e, void *thunk)
{
    AggregateCounter *ac = static_cast<AggregateCounter *>(e);
    switch ((int)thunk) {
      case 0:
	return cp_unparse_bool(ac->_frozen) + "\n";
      default:
	return "<error>";
    }
}

int
AggregateCounter::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh)
{
    AggregateCounter *ac = static_cast<AggregateCounter *>(e);
    String s = cp_uncomment(data);
    switch ((int)thunk) {
      case 0: {
	  bool val;
	  if (!cp_bool(s, &val))
	      return errh->error("argument to `frozen' should be bool");
	  ac->_frozen = val;
	  return 0;
      }
      default:
	return errh->error("internal error");
    }
}

void
AggregateCounter::add_handlers()
{
    add_write_handler("write_ascii_file", write_file_handler, (void *)0);
    add_write_handler("write_file", write_file_handler, (void *)1);
    add_read_handler("freeze", read_handler, (void *)0);
    add_write_handler("freeze", write_handler, (void *)0);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AggregateCounter)
