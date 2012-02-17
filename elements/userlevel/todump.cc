// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * todump.{cc,hh} -- element writes packets to tcpdump-like file
 * John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2007 Regents of the University of California
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
#include <click/glue.hh>
#include "todump.hh"
#include <click/args.hh>
#include <click/router.hh>
#if CLICK_NS
# include <click/master.hh>
#endif
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include "fakepcap.hh"
#include <click/userutils.hh>
CLICK_DECLS

ToDump::ToDump()
    : _fp(0), _count(0), _task(this), _use_encap_from(0)
{
}

ToDump::~ToDump()
{
}

int
ToDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String encap_type;
    String use_encap_from;
    _snaplen = 2000;
    _extra_length = true;
    _unbuffered = false;
#if CLICK_NS
    bool per_node = false;
#endif

    if (Args(conf, this, errh)
	.read_mp("FILENAME", FilenameArg(), _filename)
	.read_p("SNAPLEN", _snaplen)
	.read_p("ENCAP", WordArg(), encap_type)
	.read("USE_ENCAP_FROM", AnyArg(), use_encap_from)
	.read("EXTRA_LENGTH", _extra_length)
	.read("UNBUFFERED", _unbuffered)
#if CLICK_NS
	.read("PER_NODE", per_node)
#endif
	.complete() < 0)
	return -1;

    if (_snaplen == 0)
	_snaplen = 0xFFFFFFFFU;

    if (use_encap_from && encap_type)
	return errh->error("specify at most one of 'ENCAP' and 'USE_ENCAP_FROM'");
    else if (use_encap_from) {
	Vector<String> words;
	cp_spacevec(use_encap_from, words);
	_use_encap_from = new Element *[words.size() + 1];
	for (int i = 0; i < words.size(); i++)
	    if (!(_use_encap_from[i] = router()->find(words[i], this, errh)))
		return -1;
	_use_encap_from[words.size()] = 0;
	if (words.size() == 0)
	    return errh->error("element names missing after 'USE_ENCAP_FROM'");
    } else if (!encap_type)
	_linktype = FAKE_DLT_EN10MB;
    else if ((_linktype = fake_pcap_parse_dlt(encap_type)) < 0)
	return errh->error("bad encapsulation type");

#if CLICK_NS
    if (per_node) {
	char tmp[255];
	int r = simclick_sim_command(router()->simnode(), SIMCLICK_GET_NODE_NAME,tmp,255);
	if (r >= 0)
	    _filename = String(tmp) + String("_") + _filename;
    }
#endif

    return 0;
}

ToDump *
ToDump::hotswap_element() const
{
    if (Element *e = Element::hotswap_element())
	if (ToDump *td = (ToDump *)e->cast("ToDump"))
	    if (td->_filename == _filename
		&& td->_linktype == _linktype)
		return td;
    return 0;
}

int
ToDump::initialize(ErrorHandler *errh)
{
    // check _use_encap_from
    if (_use_encap_from) {
	_linktype = -1;
	// collect encap types
	Vector<String> encap_types;
	for (int i = 0; _use_encap_from[i]; i++) {
	    const Handler *h = Router::handler(_use_encap_from[i], "encap");
	    if (!h || !h->readable())
		return errh->error("%<%p{element}%> has no %<encap%> read handler", _use_encap_from[i]);
	    encap_types.push_back(cp_uncomment(h->call_read(_use_encap_from[i])));
	}
	// parse encap types
	for (int i = 0; i < encap_types.size(); i++) {
	    int et = fake_pcap_parse_dlt(encap_types[i]);
	    if (et < 0)
		return errh->error("%<%p{element}.encap%> did not return a valid encapsulation type", _use_encap_from[i]);
	    else if (_linktype >= 0 && et != _linktype) {
		errh->error("source encapsulation types disagree:");
		for (int j = 0; j < encap_types.size(); j++)
		    errh->error("  %s has %s\n", _use_encap_from[j]->declaration().c_str(), encap_types[j].c_str());
		return -EINVAL;
	    } else
		_linktype = et;
	}
    }

    // skip initialization if we're hotswapping later
    if (!hotswap_element()) {

	// prepare files
	assert(!_fp);
	if (_filename != "-") {
	    if (compressed_filename(_filename) > 0)
		_fp = open_compress_pipe(_filename, errh);
	    else
		_fp = fopen(_filename.c_str(), "wb");
	    if (!_fp)
		return errh->error("%s: %s", _filename.c_str(), strerror(errno));
	} else {
	    _fp = stdout;
	    _filename = "<stdout>";
	}

	if (_unbuffered)
	    setvbuf(_fp, (char *) 0, _IONBF, 0);

	struct fake_pcap_file_header h;

	h.magic = FAKE_PCAP_MAGIC;
	h.version_major = FAKE_PCAP_VERSION_MAJOR;
	h.version_minor = FAKE_PCAP_VERSION_MINOR;

	h.thiszone = 0;		// timestamps are in GMT
	h.sigfigs = 0;		// XXX accuracy of timestamps?
	h.snaplen = _snaplen;
	h.linktype = _linktype;

	size_t wrote_header = fwrite(&h, sizeof(h), 1, _fp);
	if (wrote_header != 1)
	    return errh->error("%s: unable to write file header", _filename.c_str());
    }

    if (input_is_pull(0) && noutputs() == 0) {
	ScheduleInfo::join_scheduler(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    _active = true;
    return 0;
}

void
ToDump::take_state(Element *e, ErrorHandler *)
{
    ToDump *td = static_cast<ToDump *>(e); // result of hotswap_element()
    _fp = td->_fp;
    td->_fp = 0;
}

void
ToDump::cleanup(CleanupStage)
{
    if (_fp && _fp != stdout)
	fclose(_fp);
    _fp = 0;
}

void
ToDump::write_packet(Packet *p)
{
    struct fake_pcap_pkthdr ph;

    const Timestamp& ts = p->timestamp_anno();
    if (!ts) {
	Timestamp now = Timestamp::now();
	ph.ts.tv.tv_sec = now.sec();
	ph.ts.tv.tv_usec = now.usec();
    } else {
	ph.ts.tv.tv_sec = ts.sec();
	ph.ts.tv.tv_usec = ts.usec();
    }

    unsigned to_write = p->length();
    ph.len = to_write + (_extra_length ? EXTRA_LENGTH_ANNO(p) : 0);
    if (_snaplen && to_write > _snaplen)
	to_write = _snaplen;
    ph.caplen = to_write;

    // XXX writing to pipe?
    if (fwrite(&ph, sizeof(ph), 1, _fp) == 0
	|| fwrite(p->data(), 1, to_write, _fp) == 0) {
	if (errno != EAGAIN) {
	    _active = false;
	    click_chatter("ToDump(%s): %s", _filename.c_str(), strerror(errno));
	}
    } else
	_count++;
}

void
ToDump::push(int, Packet *p)
{
    if (_active)
	write_packet(p);
    checked_output_push(0, p);
}

Packet *
ToDump::pull(int)
{
    Packet *p = input(0).pull();
    if (_active && p)
	write_packet(p);
    return p;
}

bool
ToDump::run_task(Task *)
{
    if (!_active)
	return false;
    Packet *p = input(0).pull();
    if (p) {
	write_packet(p);
	p->kill();
    } else if (!_signal)
	return false;
    _task.fast_reschedule();
    return p != 0;
}

enum { H_FILENAME = 0, H_COUNT = 1, H_RESET_COUNTS = 2 };

String
ToDump::read_handler(Element *e, void *thunk)
{
    ToDump *td = static_cast<ToDump *>(e);
    switch ((uintptr_t) thunk) {
    case H_FILENAME:
	return td->_filename;
    case H_COUNT:
	return String(td->_count);
    default:
	return "<error>";
    }
}

int
ToDump::write_handler(const String &, Element *e, void *, ErrorHandler *)
{
    ToDump *td = static_cast<ToDump *>(e);
    td->_count = 0;
    return 0;
}

void
ToDump::add_handlers()
{
    add_read_handler("filename", read_handler, H_FILENAME);
    add_read_handler("count", read_handler, H_COUNT);
    add_write_handler("reset_counts", write_handler, H_RESET_COUNTS, Handler::BUTTON);
    if (input_is_pull(0) && noutputs() == 0)
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakePcap)
EXPORT_ELEMENT(ToDump)
