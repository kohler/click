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
#include <click/handlercall.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/integers.hh>	// for first_bit_set
#include <click/router.hh>

AggregateCounter::AggregateCounter()
    : Element(1, 1), _root(0), _free(0), _call_nnz_h(0), _call_count_h(0)
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
    set_noutputs(n <= 1 ? 1 : 2);
}

int
AggregateCounter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool bytes = false;
    bool ip_bytes = false;
    bool packet_count = true;
    bool extra_length = true;
    uint32_t freeze_nnz, stop_nnz;
    uint64_t freeze_count, stop_count;
    String call_nnz, call_count;
    freeze_nnz = stop_nnz = _call_nnz = (uint32_t)(-1);
    freeze_count = stop_count = _call_count = (uint64_t)(-1);
    
    if (cp_va_parse(conf, this, errh,
		    cpKeywords,
		    "BYTES", cpBool, "count bytes?", &bytes,
		    "IP_BYTES", cpBool, "do not count link header bytes?", &ip_bytes,
		    "MULTIPACKET", cpBool, "use packet count annotation?", &packet_count,
		    "EXTRA_LENGTH", cpBool, "use extra length annotation?", &extra_length,
		    "FREEZE_AFTER_AGG", cpUnsigned, "freeze after N nonzero aggregates", &freeze_nnz,
		    "FREEZE_AFTER_COUNT", cpUnsigned64, "freeze after count reaches N", &freeze_count,
		    "STOP_AFTER_AGG", cpUnsigned, "stop router after N nonzero aggregates", &stop_nnz,
		    "STOP_AFTER_COUNT", cpUnsigned64, "stop router after count reaches N", &stop_count,
		    "CALL_AFTER_AGG", cpArgument, "call handler after N nonzero aggregates", &call_nnz,
		    "CALL_AFTER_COUNT", cpArgument, "call handler after count reaches N", &call_count,
		    "BANNER", cpString, "output banner", &_output_banner,
		    0) < 0)
	return -1;
    
    _bytes = bytes;
    _ip_bytes = ip_bytes;
    _use_packet_count = packet_count;
    _use_extra_length = extra_length;

    if ((freeze_nnz != (uint32_t)(-1)) + (stop_nnz != (uint32_t)(-1)) + ((bool)call_nnz) > 1)
	return errh->error("`FREEZE_AFTER_AGG', `STOP_AFTER_AGG', and `CALL_AFTER_AGG' are mutually exclusive");
    else if (freeze_nnz != (uint32_t)(-1)) {
	_call_nnz = freeze_nnz;
	_call_nnz_h = new HandlerCall(id() + ".freeze true");
    } else if (stop_nnz != (uint32_t)(-1)) {
	_call_nnz = stop_nnz;
	_call_nnz_h = new HandlerCall(id() + ".stop");
    } else if (call_nnz) {
	if (!cp_unsigned(cp_pop_spacevec(call_nnz), &_call_nnz))
	    return errh->error("`CALL_AFTER_AGG' first word should be unsigned (number of aggregates)");
	_call_nnz_h = new HandlerCall(call_nnz);
    }
    
    if ((freeze_count != (uint64_t)(-1)) + (stop_count != (uint64_t)(-1)) + ((bool)call_count) > 1)
	return errh->error("`FREEZE_AFTER_COUNT', `STOP_AFTER_COUNT', and `CALL_AFTER_COUNT' are mutually exclusive");
    else if (freeze_count != (uint64_t)(-1)) {
	_call_count = freeze_count;
	_call_count_h = new HandlerCall(id() + ".freeze true");
    } else if (stop_count != (uint64_t)(-1)) {
	_call_count = stop_count;
	_call_count_h = new HandlerCall(id() + ".stop");
    } else if (call_count) {
	if (!cp_unsigned64(cp_pop_spacevec(call_count), &_call_count))
	    return errh->error("`CALL_AFTER_COUNT' first word should be unsigned (count)");
	_call_count_h = new HandlerCall(call_count);
    }
    
    return 0;
}

int
AggregateCounter::initialize(ErrorHandler *errh)
{
    if (_call_nnz_h && _call_nnz_h->initialize_write(this, errh) < 0)
	return -1;
    if (_call_count_h && _call_count_h->initialize_write(this, errh) < 0)
	return -1;

    if (clear(errh) < 0)
	return -1;
    
    _frozen = false;
    _active = true;
    return 0;
}

void
AggregateCounter::cleanup(CleanupStage)
{
    for (int i = 0; i < _blocks.size(); i++)
	delete[] _blocks[i];
    _blocks.clear();
    delete _call_nnz_h;
    delete _call_count_h;
    _call_nnz_h = _call_count_h = 0;
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
    int swivel = first_bit_set(a ^ n->aggregate);
    // bitvalue is the value of that bit of 'a'
    int bitvalue = (a >> (32 - swivel)) & 1;
    // mask masks off all bits before swivel
    uint32_t mask = (swivel == 1 ? 0 : (0xFFFFFFFFU << (33 - swivel)));

    down[bitvalue]->aggregate = a;
    down[bitvalue]->count = 0;
    down[bitvalue]->child[0] = down[bitvalue]->child[1] = 0;

    *down[1 - bitvalue] = *n;	/* copy orig node down one level */

    n->aggregate = (down[0]->aggregate & mask);
    if (down[0]->aggregate == n->aggregate) {
	n->count = down[0]->count;
	down[0]->count = 0;
    } else
	n->count = 0;
    n->child[0] = down[0];	/* point to children */
    n->child[1] = down[1];

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
	    int swivel = first_bit_set(n->child[0]->aggregate ^ n->child[1]->aggregate);
	    if (first_bit_set(a ^ n->aggregate) < swivel) // input differs earlier
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

inline bool
AggregateCounter::update(Packet *p, bool frozen)
{
    if (!_active)
	return false;
    
    // AGGREGATE_ANNO is already in host byte order!
    uint32_t agg = AGGREGATE_ANNO(p);
    Node *n = find_node(agg, frozen);
    if (!n)
	return false;
    
    uint32_t amount;
    if (!_bytes)
	amount = 1 + (_use_packet_count ? EXTRA_PACKETS_ANNO(p) : 0);
    else {
	amount = p->length() + (_use_extra_length ? EXTRA_LENGTH_ANNO(p) : 0);
	if (_ip_bytes && p->network_header())
	    amount -= p->network_header_offset();
    }
    
    // update _num_nonzero; possibly call handler
    if (amount && !n->count) {
	if (_num_nonzero >= _call_nnz) {
	    _call_nnz = (uint32_t)(-1);
	    _call_nnz_h->call_write(this);
	    // handler may have changed our state; reupdate
	    return update(p, frozen || _frozen);
	}
	_num_nonzero++;
    }
    
    n->count += amount;
    _count += amount;
    if (_count >= _call_count) {
	_call_count = (uint64_t)(-1);
	_call_count_h->call_write(this);
    }
    return true;
}

void
AggregateCounter::push(int port, Packet *p)
{
    port = !update(p, _frozen || (port == 1));
    output(noutputs() == 1 ? 0 : port).push(p);
}

Packet *
AggregateCounter::pull(int port)
{
    Packet *p = input(ninputs() == 1 ? 0 : port).pull();
    if (p && _active)
	update(p, _frozen || (port == 1));
    return p;
}


// CLEAR, REAGGREGATE

void
AggregateCounter::clear_node(Node *n)
{
    if (n->child[0]) {
	clear_node(n->child[0]);
	clear_node(n->child[1]);
    }
    free_node(n);
}

int
AggregateCounter::clear(ErrorHandler *errh)
{
    if (_root)
	clear_node(_root);
    
    if (!(_root = new_node())) {
	if (errh)
	    errh->error("out of memory!");
	return -1;
    }
    _root->aggregate = 0;
    _root->count = 0;
    _root->child[0] = _root->child[1] = 0;
    _num_nonzero = 0;
    _count = 0;
    return 0;
}


void
AggregateCounter::reaggregate_node(Node *n)
{
    Node *l = n->child[0], *r = n->child[1];
    uint32_t count = n->count;
    free_node(n);
    
    if ((n = find_node(count, false))) {
	if (!n->count)
	    _num_nonzero++;
	n->count++;
	_count++;
    }

    if (l) {
	reaggregate_node(l);
	reaggregate_node(r);
    }
}

void
AggregateCounter::reaggregate_counts()
{
    Node *old_root = _root;
    _root = 0;
    clear();
    reaggregate_node(old_root);
}


// HANDLERS

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

    fwrite(_output_banner.data(), 1, _output_banner.length(), f);
    if (_output_banner.length() && _output_banner.back() != '\n')
	fputc('\n', f);
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

enum {
    AC_FROZEN, AC_ACTIVE, AC_BANNER, AC_STOP, AC_REAGGREGATE, AC_CLEAR,
    AC_CALL_AFTER_AGG, AC_CALL_AFTER_COUNT, AC_NAGG
};

String
AggregateCounter::read_handler(Element *e, void *thunk)
{
    AggregateCounter *ac = static_cast<AggregateCounter *>(e);
    switch ((int)thunk) {
      case AC_FROZEN:
	return cp_unparse_bool(ac->_frozen) + "\n";
      case AC_ACTIVE:
	return cp_unparse_bool(ac->_active) + "\n";
      case AC_BANNER:
	return ac->_output_banner;
      case AC_CALL_AFTER_AGG:
	if (ac->_call_nnz == (uint32_t)(-1))
	    return "";
	else
	    return String(ac->_call_nnz) + " " + ac->_call_nnz_h->unparse(ac);
      case AC_CALL_AFTER_COUNT:
	if (ac->_call_count == (uint64_t)(-1))
	    return "";
	else
	    return String(ac->_call_count) + " " + ac->_call_count_h->unparse(ac);
      case AC_NAGG:
	return String(ac->_num_nonzero) + "\n";
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
      case AC_FROZEN: {
	  bool val;
	  if (!cp_bool(s, &val))
	      return errh->error("argument to `frozen' should be bool");
	  ac->_frozen = val;
	  return 0;
      }
      case AC_ACTIVE: {
	  bool val;
	  if (!cp_bool(s, &val))
	      return errh->error("argument to `active' should be bool");
	  ac->_active = val;
	  return 0;
      }
      case AC_STOP:
	ac->_active = false;
	ac->router()->please_stop_driver();
	return 0;
      case AC_REAGGREGATE:
	ac->reaggregate_counts();
	return 0;
      case AC_BANNER:
	ac->_output_banner = data;
	if (data && data.back() != '\n')
	    ac->_output_banner += '\n';
	else if (data && data.length() == 1)
	    ac->_output_banner = "";
	return 0;
      case AC_CLEAR:
	return ac->clear(errh);
      case AC_CALL_AFTER_AGG:
	if (!s) {
	    ac->_call_nnz = (uint32_t)(-1);
	    return 0;
	}
	if (!ac->_call_nnz_h)
	    ac->_call_nnz_h = new HandlerCall;
	if (!cp_unsigned(cp_pop_spacevec(s), &ac->_call_nnz))
	    return errh->error("argument to `call_after_agg' should be `N HANDLER [VALUE]'");
	else if (ac->_call_nnz_h->initialize_write(s, ac, errh) < 0) {
	    ac->_call_nnz = (uint32_t)(-1);
	    return -1;
	} else
	    return 0;
      case AC_CALL_AFTER_COUNT:
	if (!s) {
	    ac->_call_count = (uint64_t)(-1);
	    return 0;
	}
	if (!ac->_call_count_h)
	    ac->_call_count_h = new HandlerCall;
	if (!cp_unsigned64(cp_pop_spacevec(s), &ac->_call_count))
	    return errh->error("argument to `call_after_count' should be `N HANDLER [VALUE]'");
	else if (ac->_call_count_h->initialize_write(s, ac, errh) < 0) {
	    ac->_call_count = (uint64_t)(-1);
	    return -1;
	} else
	    return 0;
      default:
	return errh->error("internal error");
    }
}

void
AggregateCounter::add_handlers()
{
    add_write_handler("write_ascii_file", write_file_handler, (void *)0);
    add_write_handler("write_file", write_file_handler, (void *)1);
    add_read_handler("freeze", read_handler, (void *)AC_FROZEN);
    add_write_handler("freeze", write_handler, (void *)AC_FROZEN);
    add_read_handler("active", read_handler, (void *)AC_ACTIVE);
    add_write_handler("active", write_handler, (void *)AC_ACTIVE);
    add_write_handler("stop", write_handler, (void *)AC_STOP);
    add_write_handler("reaggregate_counts", write_handler, (void *)AC_REAGGREGATE);
    add_read_handler("banner", read_handler, (void *)AC_BANNER);
    add_write_handler("banner", write_handler, (void *)AC_BANNER);
    add_write_handler("clear", write_handler, (void *)AC_CLEAR);
    add_read_handler("call_after_agg", read_handler, (void *)AC_CALL_AFTER_AGG);
    add_write_handler("call_after_agg", write_handler, (void *)AC_CALL_AFTER_AGG);
    add_read_handler("call_after_count", read_handler, (void *)AC_CALL_AFTER_COUNT);
    add_write_handler("call_after_count", write_handler, (void *)AC_CALL_AFTER_COUNT);
    add_read_handler("nagg", read_handler, (void *)AC_NAGG);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AggregateCounter)
