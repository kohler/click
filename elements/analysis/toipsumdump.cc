// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * toipsummarydump.{cc,hh} -- element writes packet summary in ASCII
 * Eddie Kohler
 *
 * Copyright (c) 2001-3 International Computer Science Institute
 * Copyright (c) 2004 Regents of the University of California
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
#include "toipsumdump.hh"
#include <click/standard/scheduleinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <unistd.h>
#include <time.h>
CLICK_DECLS

ToIPSummaryDump::ToIPSummaryDump()
    : Element(1, 0), _f(0), _task(this)
{
}

ToIPSummaryDump::~ToIPSummaryDump()
{
}

void
ToIPSummaryDump::static_initialize()
{
    IPSummaryDump::anno_register_unparsers();
    IPSummaryDump::ip_register_unparsers();
    IPSummaryDump::tcp_register_unparsers();
    IPSummaryDump::udp_register_unparsers();
}

void
ToIPSummaryDump::static_cleanup()
{
    IPSummaryDump::static_cleanup();
}

int
ToIPSummaryDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int before = errh->nerrors();
    String save = "timestamp ip_src";
    bool verbose = false;
    bool bad_packets = false;
    bool careful_trunc = true;
    bool multipacket = false;
    bool binary = false;

    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump filename", &_filename,
		    cpKeywords,
		    "CONTENTS", cpArgument, "log contents", &save,
		    "DATA", cpArgument, "log contents", &save,
		    "VERBOSE", cpBool, "be verbose?", &verbose,
		    "BANNER", cpString, "banner", &_banner,
		    "MULTIPACKET", cpBool, "output multiple packets based on packet count anno?", &multipacket,
		    "BAD_PACKETS", cpBool, "output '!bad' messages for non-IP or bad IP packets?", &bad_packets,
		    "CAREFUL_TRUNC", cpBool, "output '!bad' messages for truncated IP packets?", &careful_trunc,
		    "BINARY", cpBool, "output binary data?", &binary,
		    cpEnd) < 0)
	return -1;

    Vector<String> v;
    cp_spacevec(save, v);
    _binary_size = 4;
    for (int i = 0; i < v.size(); i++) {
	String word = cp_unquote(v[i]);
	const IPSummaryDump::Field* f = IPSummaryDump::find_field(word);
	if (!f) {
	    errh->error("unknown content type '%s'", word.c_str());
	    continue;
	}

	// remember field
	_fields.push_back(f);

	// _prepare_fields
	for (int j = 0; j < _prepare_fields.size(); j++)
	    if (_prepare_fields[j]->prepare == f->prepare)
		goto found_prepare;
	if (f->prepare)
	    _prepare_fields.push_back(f);

	// binary size
      found_prepare:
	int s = f->binary_size();
	if ((s < 0 || !f->outb) && binary)
	    errh->error("cannot use CONTENTS %s with BINARY", word.c_str());
	_binary_size += s;

	// remove _multipacket if packet count specified
	if (strcmp(f->name, "count") == 0)
	    _multipacket = false;
    }
    if (_fields.size() == 0)
	errh->error("no contents specified");
    
    _verbose = verbose;
    _bad_packets = bad_packets;
    _careful_trunc = careful_trunc;
    _multipacket = multipacket;
    _binary = binary;

    return (before == errh->nerrors() ? 0 : -1);
}

int
ToIPSummaryDump::initialize(ErrorHandler *errh)
{
    assert(!_f);
    if (_filename != "-") {
	_f = fopen(_filename.c_str(), "wb");
	if (!_f)
	    return errh->error("%s: %s", _filename.cc(), strerror(errno));
    } else {
	_f = stdout;
	_filename = "<stdout>";
    }

    if (input_is_pull(0)) {
	ScheduleInfo::join_scheduler(this, &_task, errh);
	_signal = Notifier::upstream_empty_signal(this, 0, &_task);
    }
    _active = true;
    _output_count = 0;

    // magic number
    StringAccum sa;
    sa << "!IPSummaryDump " << IPSummaryDump::MAJOR_VERSION << '.' << IPSummaryDump::MINOR_VERSION << '\n';

    if (_banner)
	sa << "!creator " << cp_quote(_banner) << '\n';
    
    // host and start time
    if (_verbose) {
	char buf[BUFSIZ];
	buf[BUFSIZ - 1] = '\0';	// ensure NUL-termination
	if (gethostname(buf, BUFSIZ - 1) >= 0)
	    sa << "!host " << buf << '\n';

	time_t when = time(0);
	const char *cwhen = ctime(&when);
	struct timeval tv;
	if (gettimeofday(&tv, 0) >= 0)
	    sa << "!runtime " << tv << " (" << String(cwhen, strlen(cwhen) - 1) << ")\n";
    }

    // data description
    sa << "!data ";
    for (int i = 0; i < _fields.size(); i++)
	sa << (i ? " " : "")
	   << (strcmp(_fields[i]->name, "ntimestamp") == 0 && !_binary ? "timestamp" : _fields[i]->name);
    sa << '\n';

    // binary marker
    if (_binary)
	sa << "!binary\n";

    // print output
    fwrite(sa.data(), 1, sa.length(), _f);

    return 0;
}

void
ToIPSummaryDump::cleanup(CleanupStage)
{
    if (_f && _f != stdout)
	fclose(_f);
    _f = 0;
}

bool
ToIPSummaryDump::summary(Packet* p, StringAccum& sa, StringAccum* bad_sa, bool force_extra_length) const
{
    IPSummaryDump::PacketDesc d(p, &sa, bad_sa, _careful_trunc, force_extra_length);

    for (int i = 0; i < _prepare_fields.size(); i++)
	_prepare_fields[i]->prepare(d);

    if (_binary) {
	sa.extend(4);
	for (int i = 0; i < _fields.size(); i++) {
	    d.clear_values();
	    bool ok = _fields[i]->extract(d, _fields[i]->thunk);
	    _fields[i]->outb(d, ok, _fields[i]->thunk);
	}
	*(reinterpret_cast<uint32_t*>(sa.data())) = htonl(sa.length());
    } else {
	for (int i = 0; i < _fields.size(); i++) {
	    if (i)
		sa << ' ';
	    if (_fields[i]->extract(d, _fields[i]->thunk) && _fields[i]->outa)
		_fields[i]->outa(d, _fields[i]->thunk);
	    else
		sa << '-';
	}
	sa << '\n';
    }

    return true;
}

void
ToIPSummaryDump::write_packet(Packet* p, int multipacket)
{
    if (multipacket > 0 && EXTRA_PACKETS_ANNO(p) > 0) {
	uint32_t count = 1 + EXTRA_PACKETS_ANNO(p);
	uint32_t total_len = p->length() + EXTRA_LENGTH_ANNO(p);
	uint32_t len = p->length();
	if (total_len < count * len)
	    total_len = count * len;

	// do timestamp stepping
	Timestamp end_timestamp = p->timestamp_anno();
	Timestamp timestamp_delta;
	if (FIRST_TIMESTAMP_ANNO(p)) {
	    timestamp_delta = (end_timestamp - FIRST_TIMESTAMP_ANNO(p)) / (count - 1);
	    p->set_timestamp_anno(FIRST_TIMESTAMP_ANNO(p));
	} else
	    timestamp_delta = Timestamp();
	
	SET_EXTRA_PACKETS_ANNO(p, 0);
	for (uint32_t i = count; i > 0; i--) {
	    uint32_t l = total_len / i;
	    SET_EXTRA_LENGTH_ANNO(p, l - len);
	    total_len -= l;
	    write_packet(p, -1);
	    if (i == 1)
		p->timestamp_anno() = end_timestamp;
	    else
		p->timestamp_anno() += timestamp_delta;
	}
	
    } else {
	_sa.clear();
	_bad_sa.clear();

	summary(p, _sa, (_bad_packets ? &_bad_sa : 0), (multipacket < 0 || EXTRA_PACKETS_ANNO(p) > 0));

	if (_bad_packets && _bad_sa)
	    write_line(_bad_sa.take_string());
	fwrite(_sa.data(), 1, _sa.length(), _f);

	_output_count++;
    }
}

void
ToIPSummaryDump::push(int, Packet *p)
{
    if (_active)
	write_packet(p, _multipacket);
    p->kill();
}

bool
ToIPSummaryDump::run_task()
{
    if (!_active)
	return false;
    if (Packet *p = input(0).pull()) {
	write_packet(p, _multipacket);
	p->kill();
	_task.fast_reschedule();
	return true;
    } else if (_signal) {
	_task.fast_reschedule();
	return false;
    } else
	return false;
}

void
ToIPSummaryDump::write_line(const String& s)
{
    if (s.length()) {
	assert(s.back() == '\n');
	if (_binary) {
	    uint32_t marker = htonl(s.length() | 0x80000000U);
	    fwrite(&marker, 4, 1, _f);
	}
	fwrite(s.data(), 1, s.length(), _f);
    }
}

void
ToIPSummaryDump::add_note(const String &s)
{
    if (s.length()) {
	int extra = 1 + (s.back() == '\n' ? 0 : 1);
	if (_binary) {
	    uint32_t marker = htonl((s.length() + extra) | 0x80000000U);
	    fwrite(&marker, 4, 1, _f);
	}
	fputc('#', _f);
	fwrite(s.data(), 1, s.length(), _f);
	if (extra > 1)
	    fputc('\n', _f);
    }
}

void
ToIPSummaryDump::flush_buffer()
{
    fflush(_f);
}

void
ToIPSummaryDump::add_handlers()
{
    if (input_is_pull(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel IPSummaryDump IPSummaryDump_Anno IPSummaryDump_IP IPSummaryDump_TCP)
EXPORT_ELEMENT(ToIPSummaryDump)
CLICK_ENDDECLS
