/*
 * toipflowdumps.{cc,hh} -- creates separate trace files for each flow
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
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
#include "toipflowdumps.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <clicknet/udp.h>
#include <clicknet/icmp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
CLICK_DECLS

ToIPFlowDumps::Flow::Flow(const Packet *p, const String &filename,
			  bool absolute_time, bool absolute_seq)
    : _next(0),
      _flowid(p), _ip_p(p->ip_header()->ip_p), _aggregate(AGGREGATE_ANNO(p)),
      _filename(filename), _outputted(false),
      _npkt(0), _nnote(0), _pkt_off(0)
{
    // use the encapsulated IP header for ICMP errors
    if (_ip_p == IP_PROTO_ICMP) {
	const icmp_generic *icmph = reinterpret_cast<const icmp_generic *>(p->transport_header());
	// should assert some things here
	const click_ip *embedded_iph = reinterpret_cast<const click_ip *>(icmph + 1);
	_flowid = IPFlowID(embedded_iph);
	_ip_p = embedded_iph->ip_p;
    }

    if (PAINT_ANNO(p) & 1)	// reverse _flowid
	_flowid = _flowid.rev();
    
    _have_first_seq[0] = _have_first_seq[1] = absolute_seq;
    _first_seq[0] = _first_seq[1] = 0;

    if (absolute_time)
	_first_timestamp = make_timeval(0, 0);
    else			// make first packet have timestamp .000001
	_first_timestamp = p->timestamp_anno() - make_timeval(0, 1);

    // sanity checks
    assert(_aggregate && (_ip_p == IP_PROTO_TCP || _ip_p == IP_PROTO_UDP));
}

int
ToIPFlowDumps::Flow::create_directories(const String &n, ErrorHandler *errh)
{
    int slash = n.find_right('/');
    if (slash <= 0)
	return 0;
    String component = n.substring(0, slash);
    if (access(component.cc(), F_OK) >= 0)
	return 0;
    else if (create_directories(component, errh) < 0)
	return -1;
    else if (mkdir(component.cc(), 0777) < 0)
	return errh->error("making directory %s: %s", component.cc(), strerror(errno));
    else
	return 0;
}

// from FromIPSummaryDump
static const char * const tcp_flags_word = "FSRPAUEW";

int
ToIPFlowDumps::Flow::output(bool done, ErrorHandler *errh)
{
    FILE *f;
    if (_filename == "-")
	f = stdout;
    else if (_outputted)
	f = fopen(_filename.cc(), "a");
    else if (create_directories(_filename, errh) < 0)
	return -1;
    else
	f = fopen(_filename.cc(), "w");
    if (!f)
	return errh->error("%s: %s", _filename.cc(), strerror(errno));

    if (!_outputted) {
	fprintf(f, "!IPSummaryDump 1.1\n!flowid %s %d %s %d %c\n",
		_flowid.saddr().s().cc(), ntohs(_flowid.sport()),
		_flowid.daddr().s().cc(), ntohs(_flowid.dport()),
		(_ip_p == IP_PROTO_TCP ? 'T' : 'U'));
	if (_ip_p == IP_PROTO_TCP)
	    fprintf(f, "!data 'timestamp' 'direction' 'tcp flags' 'tcp seq' 'tcp ack' 'payload len'\n");
	else
	    fprintf(f, "!data 'timestamp' 'direction' 'payload len'\n");
	if (_have_first_seq[0] && _first_seq[0] && _ip_p == IP_PROTO_TCP)
	    fprintf(f, "!firstseq > %u\n", _first_seq[0]);
	if (_have_first_seq[1] && _first_seq[1] && _ip_p == IP_PROTO_TCP)
	    fprintf(f, "!firstseq < %u\n", _first_seq[1]);
	if (timerisset(&_first_timestamp)) {
	    struct timeval real_firsttime = _first_timestamp + make_timeval(0, 1);
	    fprintf(f, "!firsttime %lu.%06ld\n", real_firsttime.tv_sec, real_firsttime.tv_usec);
	}
    }

    int pi = 0, ni = 0;
    while (pi < _npkt || ni < _nnote)
	if (ni >= _nnote || _note[ni].before_pkt > _pkt_off + pi) {
	    int direction = _pkt[pi].direction;
	    fprintf(f, "%lu.%06ld %c ", _pkt[pi].timestamp.tv_sec,
		    _pkt[pi].timestamp.tv_usec, (direction == 0 ? '>' : '<'));

	    if (_ip_p == IP_PROTO_TCP) {
		int flags = _pkt[pi].th_flags;
		if (flags == TH_ACK)
		    fputc('A', f);
		else if (flags == (TH_ACK | TH_PUSH))
		    fputs("PA", f);
		else if (flags == 0)
		    fputc('.', f);
		else
		    for (int flag = 0; flag < 7; flag++)
			if (flags & (1 << flag))
			    fputc(tcp_flags_word[flag], f);

		fprintf(f, " %u %u %u\n", _pkt[pi].th_seq, _pkt[pi].th_ack, _pkt[pi].payload_len);
	    } else
		fprintf(f, "%u\n", _pkt[pi].payload_len);

	    pi++;
	} else {
	    fputc('#', f);
	    int len = (ni == _nnote - 1 ? _note_text.length() : _note[ni+1].pos) - _note[ni].pos;
	    fwrite(_note_text.data() + _note[ni].pos, 1, len, f);
	    fputc('\n', f);
	    ni++;
	}

    _pkt_off += _npkt;
    _npkt = 0;
    
    _note_text.clear();
    _nnote = 0;

    if (done)
	fprintf(f, "!eof\n");
    if (f != stdout)
	fclose(f);
    _outputted = true;
    return 0;
}

int
ToIPFlowDumps::Flow::add_pkt(const Packet *p, ErrorHandler *errh)
{
    // ICMP errors are handled as notes, not packets
    if (PAINT_ANNO(p) >= 2) {
	assert(p->ip_header()->ip_p == IP_PROTO_ICMP);
	StringAccum sa;
	sa << p->timestamp_anno() << ' ' << (PAINT_ANNO(p) & 1 ? '>' : '<') << " ICMP_error";
	return add_note(sa.take_string(), errh);
    }
    
    if (_npkt >= NPKT && output(false, errh) < 0)
	return -1;
    
    int direction = (PAINT_ANNO(p) & 1);
    const click_ip *iph = p->ip_header();
    assert(iph->ip_p == _ip_p);
    
    _pkt[_npkt].timestamp = p->timestamp_anno() - _first_timestamp;
    _pkt[_npkt].direction = direction;

    if (_ip_p == IP_PROTO_TCP) {
	const click_tcp *tcph = p->tcp_header();
	
	tcp_seq_t s = ntohl(tcph->th_seq);
	if (!_have_first_seq[direction]) {
	    _first_seq[direction] = s;
	    _have_first_seq[direction] = true;
	}
	tcp_seq_t a = ntohl(tcph->th_ack);
	if (!(tcph->th_flags & TH_ACK))
	    a = _first_seq[!direction];
	else if (!_have_first_seq[!direction]) {
	    _first_seq[!direction] = a;
	    _have_first_seq[!direction] = true;
	}
	
	_pkt[_npkt].th_seq = s - _first_seq[direction];
	_pkt[_npkt].th_ack = a - _first_seq[!direction];
	_pkt[_npkt].th_flags = tcph->th_flags;
	_pkt[_npkt].payload_len = ntohs(iph->ip_len) - (iph->ip_hl << 2) - (tcph->th_off << 2); // XXX check for correctness?
    } else
	_pkt[_npkt].payload_len = ntohs(iph->ip_len) - sizeof(click_udp);
    
    _npkt++;
    
    return 0;
}

int
ToIPFlowDumps::Flow::add_note(const String &s, ErrorHandler *errh)
{
    if (_nnote >= NNOTE && output(false, errh) < 0)
	return -1;

    _note[_nnote].before_pkt = _pkt_off + _npkt;
    _note[_nnote].pos = _note_text.length();
    _note_text << s;
    _nnote++;

    return 0;
}


ToIPFlowDumps::ToIPFlowDumps()
    : Element(1, 0), _nnoagg(0), _nagg(0), _agg_notifier(0), _task(this),
      _gc_timer(gc_hook, this)
{
    MOD_INC_USE_COUNT;
    for (int i = 0; i < NFLOWMAP; i++)
	_flowmap[i] = 0;
}

ToIPFlowDumps::~ToIPFlowDumps()
{
    MOD_DEC_USE_COUNT;
}

void
ToIPFlowDumps::notify_noutputs(int n)
{
    set_noutputs(n < 1 ? 0 : 1);
}

int
ToIPFlowDumps::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Element *e = 0;
    bool absolute_time = false, absolute_seq = false;
    
    if (cp_va_parse(conf, this, errh,
		    cpOptional,
		    cpFilename, "output filename pattern", &_filename_pattern,
		    cpKeywords,
		    "OUTPUT_PATTERN", cpFilename, "output filename pattern", &_filename_pattern,
		    "NOTIFIER", cpElement, "aggregate deletion notifier", &e,
		    "ABSOLUTE_TIME", cpBool, "print absolute timestamps?", &absolute_time,
		    "ABSOLUTE_SEQ", cpBool, "print absolute sequence numbers?", &absolute_seq,
		    0) < 0)
	return -1;

    if (!_filename_pattern)
	_filename_pattern = "-";
    if (_filename_pattern.find_left('%') < 0)
	errh->warning("OUTPUT_PATTERN has no %% escapes, so output files will get overwritten");
    
    if (e && !(_agg_notifier = (AggregateNotifier *)e->cast("AggregateNotifier")))
	return errh->error("%s is not an AggregateNotifier", e->id().cc());

    _absolute_time = absolute_time;
    _absolute_seq = absolute_seq;

    return 0;
}

void
ToIPFlowDumps::cleanup(CleanupStage)
{
    for (int i = 0; i < NFLOWMAP; i++)
	while (_flowmap[i]) {
	    Flow *n = _flowmap[i]->next();
	    _flowmap[i]->output(true, ErrorHandler::default_handler());
	    delete _flowmap[i];
	    _flowmap[i] = n;
	}
    if (_nnoagg > 0 && _nagg == 0)
	ErrorHandler::default_handler()->lwarning(declaration(), "saw no packets with aggregate annotations");
}

int
ToIPFlowDumps::initialize(ErrorHandler *errh)
{
    if (input_is_pull(0) && noutputs() == 0) {
	ScheduleInfo::join_scheduler(this, &_task, errh);
	_signal = Notifier::upstream_pull_signal(this, 0, &_task);
    }
    if (_agg_notifier)
	_agg_notifier->add_listener(this);
    _gc_timer.initialize(this);
    return 0;
}

String
ToIPFlowDumps::expand_filename(const Packet *pkt, ErrorHandler *errh) const
{
    const char *data = _filename_pattern.data();
    int len = _filename_pattern.length();
    StringAccum sa;
    
    for (int p = 0; p < len; p++)
	if (data[p] == '%') {
	    p++;
	    bool zero_pad = false;
	    int field_width = -1;
	    int precision = -1;
	    if (p < len && data[p] == '0')
		zero_pad = true, p++;
	    if (p < len && isdigit(data[p])) {
		field_width = data[p] - '0';
		for (p++; p < len && isdigit(data[p]); p++)
		    field_width = (field_width * 10) + data[p] - '0';
	    }
	    if (p < len && data[p] == '.') {
		precision = 0;
		for (p++; p < len && isdigit(data[p]); p++)
		    precision = (precision * 10) + data[p] - '0';
	    }
	    
	    StringAccum subsa;
	    if (p >= len)
		errh->error("bad filename pattern");
	    else if (data[p] == 'n' || data[p] == 'x' || data[p] == 'X') {
		char format[3] = "%d";
		if (data[p] != 'n')
		    format[1] = data[p];
		uint32_t value = AGGREGATE_ANNO(pkt);
		if (precision >= 0 && precision <= 3)
		    value = (value >> ((3 - precision) * 8)) & 255;
		else if (precision >= 4 && precision <= 5)
		    value = (value >> ((5 - precision) * 16)) & 65535;
		subsa.snprintf(20, format, value);
	    } else if (data[p] == 's' && (precision < 0 || precision > 3))
		subsa << IPAddress(pkt->ip_header()->ip_src);
	    else if (data[p] == 's')
		subsa << ((ntohl(pkt->ip_header()->ip_src.s_addr) >> ((3 - precision) * 8)) & 255);
	    else if (data[p] == 'd' && (precision < 0 || precision > 3))
		subsa << IPAddress(pkt->ip_header()->ip_dst);
	    else if (data[p] == 'd')
		subsa << ((ntohl(pkt->ip_header()->ip_dst.s_addr) >> ((3 - precision) * 8)) & 255);
	    else if (data[p] == 'S')
		subsa << ntohs(pkt->tcp_header()->th_sport);
	    else if (data[p] == 'D')
		subsa << ntohs(pkt->tcp_header()->th_dport);
	    else if (data[p] == 'p')
		subsa << (pkt->ip_header()->ip_p == IP_PROTO_TCP ? 'T' : 'U');
	    else if (data[p] == '%')
		subsa << '%';
	    else
		errh->error("bad filename pattern `%%%c'", data[p]);
	    
	    if (field_width >= 0 && subsa.length() < field_width)
		for (int l = field_width - subsa.length(); l > 0; l--)
		    sa << (zero_pad ? '0' : '_');
	    sa << subsa;
	} else
	    sa << _filename_pattern[p];

    return sa.take_string();
}

ToIPFlowDumps::Flow *
ToIPFlowDumps::find_aggregate(uint32_t agg, const Packet *p)
{
    if (agg == 0)
	return 0;
    
    int bucket = (agg & (NFLOWMAP - 1));
    Flow *prev = 0, *f = _flowmap[bucket];
    while (f && f->aggregate() != agg) {
	prev = f;
	f = f->next();
    }

    if (f)
	/* nada */;
    else if (p && (f = new Flow(p, expand_filename(p, ErrorHandler::default_handler()), _absolute_time, _absolute_seq)))
	prev = f;
    else
	return 0;

    if (prev) {
	prev->set_next(f->next());
	f->set_next(_flowmap[bucket]);
	_flowmap[bucket] = f;
    }
    
    return f;
}

inline void
ToIPFlowDumps::smaction(Packet *p)
{
    if (Flow *f = find_aggregate(AGGREGATE_ANNO(p), p)) {
	_nagg++;
	f->add_pkt(p, ErrorHandler::default_handler());
    } else
	_nnoagg++;
}

void
ToIPFlowDumps::push(int, Packet *p)
{
    smaction(p);
    checked_output_push(0, p);
}

Packet *
ToIPFlowDumps::pull(int)
{
    if (Packet *p = input(0).pull()) {
	smaction(p);
	return p;
    } else
	return 0;
}

void
ToIPFlowDumps::run_scheduled()
{
    if (Packet *p = input(0).pull()) {
	smaction(p);
	p->kill();
    } else if (!_signal)
	return;
    _task.fast_reschedule();
}

void
ToIPFlowDumps::add_note(uint32_t agg, const String &s, ErrorHandler *errh)
{
    if (Flow *f = find_aggregate(agg, 0))
	f->add_note(s, (errh ? errh : ErrorHandler::default_handler()));
    else if (errh)
	errh->warning("aggregate not found");
}

void
ToIPFlowDumps::aggregate_notify(uint32_t agg, AggregateEvent event, const Packet *pkt)
{
    if (event == NEW_AGG)
	(void) find_aggregate(agg, pkt);
    else if (event != DELETE_AGG)
	return;
    else if (find_aggregate(agg, 0)) {
	_gc_aggs.push_back(agg);
	_gc_aggs.push_back(click_jiffies());
	if (!_gc_timer.scheduled())
	    _gc_timer.schedule_after_ms(250);
    }
}

void
ToIPFlowDumps::gc_hook(Timer *t, void *thunk)
{
    ToIPFlowDumps *td = reinterpret_cast<ToIPFlowDumps *>(thunk);
    uint32_t limit_jiff = click_jiffies() - (CLICK_HZ / 4);
    int i;
    for (i = 0; i < td->_gc_aggs.size() && SEQ_LEQ(td->_gc_aggs[i+1], limit_jiff); i += 2)
	if (Flow *f = td->find_aggregate(td->_gc_aggs[i], 0)) {
	    f->output(true, ErrorHandler::default_handler());
	    int bucket = (f->aggregate() & (NFLOWMAP - 1));
	    assert(td->_flowmap[bucket] == f);
	    td->_flowmap[bucket] = f->next();
	    delete f;
	}
    if (i < td->_gc_aggs.size()) {
	memmove(&td->_gc_aggs[0], &td->_gc_aggs[i], (td->_gc_aggs.size() - i) * sizeof(int));
	td->_gc_aggs.resize(td->_gc_aggs.size() - i);
	t->schedule_after_ms(250);
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ToIPFlowDumps)
