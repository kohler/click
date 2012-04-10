// -*- c-basic-offset: 4 -*-
/*
 * queueyanktest.{cc,hh} -- test Queue yank() functionality
 * Eddie Kohler
 *
 * Copyright (c) 2003 International Computer Science Institute
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
#include "queueyanktest.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

QueueYankTest::QueueYankTest()
    : _t(this)
{
}

int
QueueYankTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e;

    if (Args(conf, this, errh).read_mp("QUEUE", e).complete() < 0)
	return -1;

    if (!(_q = static_cast<SimpleQueue *>(e->cast("SimpleQueue"))))
	return errh->error("QUEUE argument must be a Queue element");

    return 0;
}

int
QueueYankTest::initialize(ErrorHandler *)
{
    _t.initialize(this);
    _t.schedule_now();
    return 0;
}

#define CHECK(x) if (!(x)) { errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x); return; }
#define CHECK_PKT(p, c) do { Packet *p_ = (p); if (!p_ || p_->data()[0] != (c)) { errh->error("%s:%d: test '%s' produced %s packet (expected '%c')", __FILE__, __LINE__, #p, (p_ ? "wrong" : "no"), (c)); return; } p_->kill(); } while (0)
#define CHECK_NOPKT(p) do { Packet *p_ = (p); if (p_) { errh->error("%s:%d: test '%s' produced a packet (expected nothing)", __FILE__, __LINE__, #p); return; } } while (0)
#define CHECK_DEQ(s) do { for (const char *s_ = (s); *s_; s_++) CHECK_PKT(_q->deq(), *s_); } while (0)
#define PREPARE_Q() do { _q->enq(a->clone()); _q->enq(b->clone()); _q->enq(c->clone()); _q->enq(d->clone()); _q->enq(e->clone()); CHECK(_q->size() == 5); } while (0)

namespace {

struct Foo {
    const unsigned char *s;
    Foo(const char *ss) : s(reinterpret_cast<const unsigned char *>(ss)) { }
    bool operator()(const Packet *p) {
	for (const unsigned char *ss = s; *ss; ss++)
	    if (p->data()[0] == *ss)
		return true;
	return false;
    }
};

}

void
QueueYankTest::run_timer(Timer *)
{
    PrefixErrorHandler perrh(ErrorHandler::default_handler(), declaration() + ": ");
    ErrorHandler *errh = &perrh;

    Packet *a = Packet::make("a", 1);
    Packet *b = Packet::make("b", 1);
    Packet *c = Packet::make("c", 1);
    Packet *d = Packet::make("d", 1);
    Packet *e = Packet::make("e", 1);

    CHECK(_q->size() == 0);

    PREPARE_Q();
    CHECK_DEQ("abcde");

    PREPARE_Q();
    CHECK_PKT(_q->yank1(Foo("c")), 'c');
    CHECK(_q->size() == 4);
    CHECK_DEQ("abde");

    PREPARE_Q();
    CHECK_NOPKT(_q->yank1(Foo("f")));
    CHECK_DEQ("abcde");

    PREPARE_Q();
    CHECK_DEQ("abcd");
    CHECK_PKT(_q->yank1(Foo("hgkje")), 'e');
    CHECK(_q->size() == 0);

    Vector<Packet *> v;
    PREPARE_Q();
    _q->yank(Foo("ade"), v);
    CHECK(v.size() == 3);
    CHECK(_q->size() == 2);
    CHECK_PKT(v[0], 'e');
    CHECK_PKT(v[1], 'd');
    CHECK_PKT(v[2], 'a');
    CHECK_DEQ("bc");

    v.clear();
    PREPARE_Q();
    _q->yank(Foo("fhq"), v);
    CHECK(v.size() == 0);
    CHECK_DEQ("abcde");

    v.clear();
    PREPARE_Q();
    _q->yank(Foo("edcba"), v);
    CHECK(v.size() == 5);
    CHECK(_q->size() == 0);
    CHECK_PKT(v[0], 'e');
    CHECK_PKT(v[1], 'd');
    CHECK_PKT(v[2], 'c');
    CHECK_PKT(v[3], 'b');
    CHECK_PKT(v[4], 'a');

    CHECK(_q->size() == 0);
    a->kill();
    b->kill();
    c->kill();
    d->kill();
    e->kill();
    errh->message("All tests pass!");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(QueueYankTest)
