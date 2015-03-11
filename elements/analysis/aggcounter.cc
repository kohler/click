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
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/integers.hh>	// for first_bit_set
#include <click/router.hh>
CLICK_DECLS

AggregateCounter::AggregateCounter()
    : _root(0), _free(0), _call_nnz_h(0), _call_count_h(0)
{
}

AggregateCounter::~AggregateCounter()
{
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

    if (Args(conf, this, errh)
	.read("BYTES", bytes)
	.read("IP_BYTES", ip_bytes)
	.read("MULTIPACKET", packet_count)
	.read("EXTRA_LENGTH", extra_length)
	.read("AGGREGATE_FREEZE", freeze_nnz)
	.read("COUNT_FREEZE", freeze_count)
	.read("AGGREGATE_STOP", stop_nnz)
	.read("COUNT_STOP", stop_count)
	.read("AGGREGATE_CALL", AnyArg(), call_nnz)
	.read("COUNT_CALL", AnyArg(), call_count)
	.read("BANNER", _output_banner).complete() < 0)
	return -1;

    _bytes = bytes;
    _ip_bytes = ip_bytes;
    _use_packet_count = packet_count;
    _use_extra_length = extra_length;

    if ((freeze_nnz != (uint32_t)(-1)) + (stop_nnz != (uint32_t)(-1)) + ((bool)call_nnz) > 1)
	return errh->error("'AGGREGATE_FREEZE', 'AGGREGATE_STOP', and 'AGGREGATE_CALL' are mutually exclusive");
    else if (freeze_nnz != (uint32_t)(-1)) {
	_call_nnz = freeze_nnz;
	_call_nnz_h = new HandlerCall(name() + ".freeze true");
    } else if (stop_nnz != (uint32_t)(-1)) {
	_call_nnz = stop_nnz;
	_call_nnz_h = new HandlerCall(name() + ".stop");
    } else if (call_nnz) {
	if (!IntArg().parse(cp_shift_spacevec(call_nnz), _call_nnz))
	    return errh->error("AGGREGATE_CALL first word should be unsigned (number of aggregates)");
	_call_nnz_h = new HandlerCall(call_nnz);
    }

    if ((freeze_count != (uint64_t)(-1)) + (stop_count != (uint64_t)(-1)) + ((bool)call_count) > 1)
	return errh->error("'COUNT_FREEZE', 'COUNT_STOP', and 'COUNT_CALL' are mutually exclusive");
    else if (freeze_count != (uint64_t)(-1)) {
	_call_count = freeze_count;
	_call_count_h = new HandlerCall(name() + ".freeze true");
    } else if (stop_count != (uint64_t)(-1)) {
	_call_count = stop_count;
	_call_count_h = new HandlerCall(name() + ".stop");
    } else if (call_count) {
	if (!IntArg().parse(cp_shift_spacevec(call_count), _call_count))
	    return errh->error("COUNT_CALL first word should be unsigned (count)");
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
    int swivel = ffs_msb(a ^ n->aggregate);
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
	    int swivel = ffs_msb(n->child[0]->aggregate ^ n->child[1]->aggregate);
	    if (ffs_msb(a ^ n->aggregate) < swivel) // input differs earlier
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
	if (_ip_bytes && p->has_network_header())
	    amount -= p->network_header_offset();
    }

    // update _num_nonzero; possibly call handler
    if (amount && !n->count) {
	if (_num_nonzero >= _call_nnz) {
	    _call_nnz = (uint32_t)(-1);
	    _call_nnz_h->call_write();
	    // handler may have changed our state; reupdate
	    return update(p, frozen || _frozen);
	}
	_num_nonzero++;
    }

    n->count += amount;
    _count += amount;
    if (_count >= _call_count) {
	_call_count = (uint64_t)(-1);
	_call_count_h->call_write();
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
write_batch(FILE *f, AggregateCounter::WriteFormat format,
	    uint32_t *buffer, int pos, double count, ErrorHandler *)
{
    if (format == AggregateCounter::WR_BINARY)
	ignore_result(fwrite(buffer, sizeof(uint32_t), pos, f));
    else if (format == AggregateCounter::WR_TEXT_IP)
	for (int i = 0; i < pos; i += 2)
	    fprintf(f, "%d.%d.%d.%d %u\n", (buffer[i] >> 24) & 255, (buffer[i] >> 16) & 255, (buffer[i] >> 8) & 255, buffer[i] & 255, buffer[i+1]);
    else if (format == AggregateCounter::WR_TEXT_PDF)
	for (int i = 0; i < pos; i += 2)
	    fprintf(f, "%u %.12g\n", buffer[i], buffer[i+1] / count);
    else if (format == AggregateCounter::WR_TEXT)
	for (int i = 0; i < pos; i += 2)
	    fprintf(f, "%u %u\n", buffer[i], buffer[i+1]);
}

void
AggregateCounter::write_nodes(Node *n, FILE *f, WriteFormat format,
			      uint32_t *buffer, int &pos, int len,
			      ErrorHandler *errh) const
{
    if (n->count > 0) {
	buffer[pos++] = n->aggregate;
	buffer[pos++] = n->count;
	if (pos == len) {
	    write_batch(f, format, buffer, pos, _count, errh);
	    pos = 0;
	}
    }

    if (n->child[0])
	write_nodes(n->child[0], f, format, buffer, pos, len, errh);
    if (n->child[1])
	write_nodes(n->child[1], f, format, buffer, pos, len, errh);
}

int
AggregateCounter::write_file(String where, WriteFormat format,
			     ErrorHandler *errh) const
{
    FILE *f;
    if (where == "-")
	f = stdout;
    else
	f = fopen(where.c_str(), (format == WR_BINARY ? "wb" : "w"));
    if (!f)
	return errh->error("%s: %s", where.c_str(), strerror(errno));

    fprintf(f, "!IPAggregate 1.0\n");
    ignore_result(fwrite(_output_banner.data(), 1, _output_banner.length(), f));
    if (_output_banner.length() && _output_banner.back() != '\n')
	fputc('\n', f);
    fprintf(f, "!num_nonzero %u\n", _num_nonzero);
    if (format == WR_BINARY) {
#if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
	fprintf(f, "!packed_be\n");
#elif CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
	fprintf(f, "!packed_le\n");
#else
	format = WR_TEXT;
#endif
    } else if (format == WR_TEXT_IP)
	fprintf(f, "!ip\n");

    uint32_t buf[1024];
    int pos = 0;
    write_nodes(_root, f, format, buf, pos, 1024, errh);
    if (pos)
	write_batch(f, format, buf, pos, _count, errh);

    bool had_err = ferror(f);
    if (f != stdout)
	fclose(f);
    if (had_err)
	return errh->error("%s: file error", where.c_str());
    else
	return 0;
}

int
AggregateCounter::write_file_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh)
{
    AggregateCounter *ac = static_cast<AggregateCounter *>(e);
    String fn;
    if (!FilenameArg().parse(cp_uncomment(data), fn))
	return errh->error("argument should be filename");
    int int_thunk = (intptr_t)thunk;
    return ac->write_file(fn, (WriteFormat)int_thunk, errh);
}

enum {
    AC_FROZEN, AC_ACTIVE, AC_BANNER, AC_STOP, AC_REAGGREGATE, AC_CLEAR,
    AC_AGGREGATE_CALL, AC_COUNT_CALL, AC_NAGG, AC_COUNT
};

String
AggregateCounter::read_handler(Element *e, void *thunk)
{
    AggregateCounter *ac = static_cast<AggregateCounter *>(e);
    switch ((intptr_t)thunk) {
      case AC_BANNER:
	return ac->_output_banner;
      case AC_AGGREGATE_CALL:
	if (ac->_call_nnz == (uint32_t)(-1))
	    return "";
	else
	    return String(ac->_call_nnz) + " " + ac->_call_nnz_h->unparse();
      case AC_COUNT_CALL:
	if (ac->_call_count == (uint64_t)(-1))
	    return "";
	else
	    return String(ac->_call_count) + " " + ac->_call_count_h->unparse();
      case AC_COUNT:
	return String(ac->_count);
      case AC_NAGG:
	return String(ac->_num_nonzero);
      default:
	return "<error>";
    }
}

int
AggregateCounter::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh)
{
    AggregateCounter *ac = static_cast<AggregateCounter *>(e);
    String s = cp_uncomment(data);
    switch ((intptr_t)thunk) {
      case AC_FROZEN: {
	  bool val;
	  if (!BoolArg().parse(s, val))
	      return errh->error("type mismatch");
	  ac->_frozen = val;
	  return 0;
      }
      case AC_ACTIVE: {
	  bool val;
	  if (!BoolArg().parse(s, val))
	      return errh->error("type mismatch");
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
      case AC_AGGREGATE_CALL: {
	  uint32_t new_nnz = (uint32_t)(-1);
	  if (s) {
	      if (!IntArg().parse(cp_shift_spacevec(s), new_nnz))
		  return errh->error("argument to 'aggregate_call' should be 'N HANDLER [VALUE]'");
	      else if (HandlerCall::reset_write(ac->_call_nnz_h, s, ac, errh) < 0)
		  return -1;
	  }
	  ac->_call_nnz = new_nnz;
	  return 0;
      }
      case AC_COUNT_CALL: {
	  uint64_t new_count = (uint64_t)(-1);
	  if (s) {
	      if (!IntArg().parse(cp_shift_spacevec(s), new_count))
		  return errh->error("argument to 'count_call' should be 'N HANDLER [VALUE]'");
	      else if (HandlerCall::reset_write(ac->_call_count_h, s, ac, errh) < 0)
		  return -1;
	  }
	  ac->_call_count = new_count;
	  return 0;
      }
      default:
	return errh->error("internal error");
    }
}

void
AggregateCounter::add_handlers()
{
    add_write_handler("write_text_file", write_file_handler, WR_TEXT);
    add_write_handler("write_ascii_file", write_file_handler, WR_TEXT);
    add_write_handler("write_file", write_file_handler, WR_BINARY);
    add_write_handler("write_ip_file", write_file_handler, WR_TEXT_IP);
    add_write_handler("write_pdf_file", write_file_handler, WR_TEXT_PDF);
    add_data_handlers("freeze", Handler::f_read | Handler::f_checkbox, &_frozen);
    add_write_handler("freeze", write_handler, AC_FROZEN);
    add_data_handlers("active", Handler::f_read | Handler::f_checkbox, &_active);
    add_write_handler("active", write_handler, AC_ACTIVE);
    add_write_handler("stop", write_handler, AC_STOP, Handler::f_button);
    add_write_handler("reaggregate_counts", write_handler, AC_REAGGREGATE);
    add_write_handler("counts_pdf", write_handler, AC_REAGGREGATE);
    add_read_handler("banner", read_handler, AC_BANNER);
    add_write_handler("banner", write_handler, AC_BANNER, Handler::f_raw);
    add_write_handler("clear", write_handler, AC_CLEAR);
    add_read_handler("aggregate_call", read_handler, AC_AGGREGATE_CALL);
    add_write_handler("aggregate_call", write_handler, AC_AGGREGATE_CALL);
    add_read_handler("count_call", read_handler, AC_COUNT_CALL);
    add_write_handler("count_call", write_handler, AC_COUNT_CALL);
    add_read_handler("count", read_handler, AC_COUNT);
    add_read_handler("nagg", read_handler, AC_NAGG);
}

ELEMENT_REQUIRES(userlevel int64)
EXPORT_ELEMENT(AggregateCounter)
CLICK_ENDDECLS
