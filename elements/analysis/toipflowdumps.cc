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
#include <sys/wait.h>
#include <fcntl.h>
#include "elements/analysis/toipsumdump.hh"
CLICK_DECLS

#ifdef i386
# define PUT4(p, d)	*reinterpret_cast<uint32_t *>((p)) = htonl((d))
#else
# define PUT4(p, d)	do { (p)[0] = (d)>>24; (p)[1] = (d)>>16; (p)[2] = (d)>>8; (p)[3] = (d); } while (0)
#endif
#define PUT1(p, d)	((p)[0] = (d))

ToIPFlowDumps::Flow::Flow(const Packet *p, const String &filename,
			  bool absolute_time, bool absolute_seq, bool binary,
			  bool ip_id, bool tcp_opt, bool tcp_window)
    : _next(0),
      _flowid(p), _ip_p(p->ip_header()->ip_p),
      _aggregate(AGGREGATE_ANNO(p)), _packet_count(0), _note_count(0),
      _filename(filename), _outputted(false), _binary(binary),
      _tcp_opt(tcp_opt), _npkt(0), _nnote(0)
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

    if (ip_id)
	_ip_ids = new uint16_t[NPKT];
    else
	_ip_ids = 0;

    if (tcp_window && _ip_p == IP_PROTO_TCP)
	_tcp_windows = new uint16_t[NPKT];
    else
	_tcp_windows = 0;
    
    // sanity checks
    assert(_aggregate && (_ip_p == IP_PROTO_TCP || _ip_p == IP_PROTO_UDP));
}

ToIPFlowDumps::Flow::~Flow()
{
    delete[] _ip_ids;
    delete[] _tcp_windows;
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

void
ToIPFlowDumps::Flow::output_binary(StringAccum &sa)
{
    union {
	uint32_t u[8];
	uint16_t us[16];
	char c[32];
    } buf;
    int pi = 0, ni = 0;
    const uint16_t *opt = reinterpret_cast<const uint16_t *>(_opt_info.data());
    const uint16_t *end_opt = opt + (_opt_info.length() / 2);
    
    while (pi < _npkt || ni < _nnote)
	if (ni >= _nnote || _note[ni].before_pkt > pi) {
	    int pos;

	    buf.u[1] = ntohl(_pkt[pi].timestamp.tv_sec);
	    buf.u[2] = ntohl(_pkt[pi].timestamp.tv_usec);
	    if (_ip_p == IP_PROTO_TCP) {
		buf.u[3] = ntohl(_pkt[pi].th_seq);
		buf.u[4] = ntohl(_pkt[pi].payload_len);
		buf.u[5] = ntohl(_pkt[pi].th_ack);
		pos = 24;
	    } else {
		buf.u[3] = ntohl(_pkt[pi].payload_len);
		pos = 16;
	    }

	    if (_ip_ids)
		buf.us[pos>>1] = _ip_ids[pi], pos += 2;
	    if (_tcp_windows)
		buf.us[pos>>1] = _tcp_windows[pi], pos += 2;
	    if (_ip_p == IP_PROTO_TCP)
		buf.c[pos++] = _pkt[pi].th_flags;
	    buf.c[pos++] = _pkt[pi].direction;
	    
	    buf.u[0] = ntohl(pos);
	    sa.append(&buf.c[0], pos);

	    // handle TCP options specially
	    if (opt < end_opt && opt[0] == pi) {
		int original_pos = sa.length() - pos;
		ToIPSummaryDump::store_tcp_opt_binary(reinterpret_cast<const uint8_t *>(opt + 2), opt[1], ToIPSummaryDump::DO_TCPOPT_MSS | ToIPSummaryDump::DO_TCPOPT_WSCALE | ToIPSummaryDump::DO_TCPOPT_SACK, sa);
		PUT4(sa.data() + original_pos, sa.length() - original_pos);
		opt += 2 + (opt[1] / 2);
	    }

	    pi++;
	} else {
	    int len = (ni == _nnote - 1 ? _note_text.length() : _note[ni+1].pos) - _note[ni].pos;
	    int record_len = (4 + len + 2);
	    buf.u[0] = ntohl(record_len | 0x80000000U);
	    buf.c[4] = '#';
	    sa.append(&buf.c[0], 5);
	    sa.append(_note_text.data() + _note[ni].pos, len);
	    sa.append("\n", 1);
	    ni++;
	}
}

int
ToIPFlowDumps::Flow::output(ErrorHandler *errh)
{
    int fd;
    if (_filename == "-")
	fd = STDOUT_FILENO;
    else if (_outputted)
	fd = open(_filename.cc(), O_WRONLY | O_APPEND);
    else if (create_directories(_filename, errh) < 0)
	return -1;
    else
	fd = open(_filename.cc(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
	return errh->error("%s: %s", _filename.cc(), strerror(errno));

    // make a guess about how much data we'll need
    StringAccum sa(_npkt * (_binary ? 28 : 40) + _note_text.length() + _nnote * 8 + _opt_info.length() + 16);
    
    if (!_outputted) {
	sa << "!IPSummaryDump 1.1\n!flowid "
	   << _flowid.saddr() << ' ' << ntohs(_flowid.sport()) << ' '
	   << _flowid.daddr() << ' ' << ntohs(_flowid.dport()) << ' '
	   << (_ip_p == IP_PROTO_TCP ? 'T' : 'U')
	   << "\n!aggregate " << _aggregate << '\n';

	sa << "!data timestamp";
	if (_binary) {
	    if (_ip_p == IP_PROTO_TCP)
		sa << " tcp_seq payload_len tcp_ack";
	    else
		sa << " payload_len";
	    if (_ip_ids)
		sa << " ip_id";
	    if (_tcp_windows)
		sa << " tcp_window";
	    if (_ip_p == IP_PROTO_TCP)
		sa << " tcp_flags";
	    sa << " direction";
	    if (_ip_p == IP_PROTO_TCP && _tcp_opt)
		sa << " tcp_opt";
	} else {
	    sa << " direction";
	    if (_ip_ids)
		sa << " ip_id";
	    if (_ip_p == IP_PROTO_TCP) {
		sa << " tcp_flags tcp_seq payload_len tcp_ack";
		if (_tcp_opt)
		    sa << " tcp_opt";
	    } else
		sa << " payload_len";
	}
	sa << '\n';
	
	if (_have_first_seq[0] && _first_seq[0] && _ip_p == IP_PROTO_TCP)
	    sa << "!firstseq > " << _first_seq[0] << '\n';
	if (_have_first_seq[1] && _first_seq[1] && _ip_p == IP_PROTO_TCP)
	    sa << "!firstseq < " << _first_seq[1] << '\n';
	if (timerisset(&_first_timestamp))
	    sa << "!firsttime " << _first_timestamp << '\n';
	if (_binary)
	    sa << "!binary\n";
    }

    if (_binary)
	output_binary(sa);
    else {
	int pi = 0, ni = 0;
	const uint16_t *opt = reinterpret_cast<const uint16_t *>(_opt_info.data());
	const uint16_t *end_opt = opt + (_opt_info.length() / 2);
	
	while (pi < _npkt || ni < _nnote)
	    if (ni >= _nnote || _note[ni].before_pkt > pi) {
		int direction = _pkt[pi].direction;
		sa << _pkt[pi].timestamp << ' '
		   << (direction == 0 ? '>' : '<') << ' ';

		if (_ip_ids)
		    sa << _ip_ids[pi] << ' ';
		
		if (_ip_p == IP_PROTO_TCP) {
		    int flags = _pkt[pi].th_flags;
		    if (flags == TH_ACK)
			sa << 'A';
		    else if (flags == (TH_ACK | TH_PUSH))
			sa << 'P' << 'A';
		    else if (flags == 0)
			sa << '.';
		    else
			for (int flag = 0; flag < 7; flag++)
			    if (flags & (1 << flag))
				sa << tcp_flags_word[flag];

		    sa << ' ' << _pkt[pi].th_seq
		       << ' ' << _pkt[pi].payload_len
		       << ' ' << _pkt[pi].th_ack;

		    if (_tcp_windows)
			sa << ' ' << ntohs(_tcp_windows[pi]);
		    
		    if (opt < end_opt && opt[0] == pi) {
			sa << ' ';
			ToIPSummaryDump::store_tcp_opt_ascii(reinterpret_cast<const uint8_t *>(opt + 2), opt[1], ToIPSummaryDump::DO_TCPOPT_MSS | ToIPSummaryDump::DO_TCPOPT_WSCALE | ToIPSummaryDump::DO_TCPOPT_SACK, sa);
			opt += 2 + (opt[1] / 2);
		    }
		    
		    sa << '\n';
		} else
		    sa << _pkt[pi].payload_len << '\n';

		pi++;
	    } else {
		int len = (ni == _nnote - 1 ? _note_text.length() : _note[ni+1].pos) - _note[ni].pos;
		sa << '#';
		sa.append(_note_text.data() + _note[ni].pos, len);
		sa << '\n';
		ni++;
	    }
    }

    _npkt = 0;
    _opt_info.clear();
    
    _note_text.clear();
    _nnote = 0;

    // actually write data
    int pos = 0;
    while (pos < sa.length()) {
	int written = write(fd, sa.data() + pos, sa.length() - pos);
	if (written < 0 && errno != EINTR) {
	    errh->error("%s: %s", _filename.cc(), strerror(errno));
	    break;
	}
	pos += written;
    }
    
    if (fd != STDOUT_FILENO)
	close(fd);
    _outputted = true;
    return 0;
}

void
ToIPFlowDumps::Flow::compress(ErrorHandler *errh)
{
    if (_filename != "-") {
	StringAccum cmd;
	cmd << "gzip -c <" << _filename << " >" + _filename << ".gz 2>/dev/null";
	int retval = system(cmd.cc());
	if (retval < 0)
	    errh->error("%s: gzip: %s", _filename.cc(), strerror(errno));
	else if (WEXITSTATUS(retval) != 0)
	    errh->error("%s: gzip exited with status %d", _filename.cc(), WEXITSTATUS(retval));
	else
	    ::unlink(_filename.cc());
    }
}

inline void
ToIPFlowDumps::Flow::unlink(ErrorHandler *errh)
{
    if (_outputted && ::unlink(_filename.cc()) < 0)
	errh->error("%s: %s", _filename.cc(), strerror(errno));
}

void
ToIPFlowDumps::Flow::store_opt(const click_tcp *tcph, int direction)
{
    const uint8_t *opt = reinterpret_cast<const uint8_t *>(tcph + 1);
    const uint8_t *end_opt = opt + ((tcph->th_off << 2) - sizeof(click_tcp));
    bool any = false;
    int original_len = _opt_info.length();
    char *data;
    
    while (opt < end_opt)
	switch (*opt) {
	  case TCPOPT_EOL:
	    goto done;
	  case TCPOPT_NOP:
	    opt++;
	    break;
	  case TCPOPT_MAXSEG:
	    if (opt[1] != TCPOLEN_MAXSEG || opt + opt[1] > end_opt)
		goto bad_opt;
	    else
		goto good_opt;
	  case TCPOPT_WSCALE:
	    if (opt[1] != TCPOLEN_WSCALE || opt + opt[1] > end_opt)
		goto bad_opt;
	    else
		goto good_opt;
	  case TCPOPT_SACK_PERMITTED:
	    if (opt[1] != TCPOLEN_SACK_PERMITTED || opt + opt[1] > end_opt)
		goto bad_opt;
	    else
		goto good_opt;
	  good_opt:
	    if (!any && (data = _opt_info.extend(4)))
		*(reinterpret_cast<uint16_t *>(data)) = _npkt;
	    if ((data = _opt_info.extend(opt[1])))
		memcpy(data, opt, opt[1]);
	    opt += opt[1];
	    any = true;
	    break;
	  case TCPOPT_SACK:
	    if (opt[1] % 8 != 2 || opt + opt[1] > end_opt)
		goto bad_opt;
	    if (!any && (data = _opt_info.extend(4)))
		*(reinterpret_cast<uint16_t *>(data)) = _npkt;
	    if ((data = _opt_info.extend(opt[1]))) {
		// argh... must number sequence numbers in sack blocks 
		memcpy(data, opt, 2);
		const uint8_t *end_sack_opt = opt + opt[1];
		for (opt += 2, data += 2; opt < end_sack_opt; opt += 8, data += 8) {
		    uint32_t buf[2];
		    memcpy(buf, opt, 8);
		    if (!_have_first_seq[!direction]) {
			_first_seq[!direction] = ntohl(buf[0]);
			_have_first_seq[!direction] = true;
		    }
		    buf[0] = htonl(ntohl(buf[0]) - _first_seq[!direction]);
		    buf[1] = htonl(ntohl(buf[1]) - _first_seq[!direction]);
		    memcpy(data, buf, 8);
		}
	    } else
		opt += opt[1];
	    any = true;
	    break;
	  default:
	    if (opt[1] == 0 || opt + opt[1] > end_opt)
		goto bad_opt;
	    opt += opt[1];
	    break;
	}

  done:
    if (any) {
	if (_opt_info.length() & 1)
	    _opt_info.append('\0');
	*(reinterpret_cast<uint16_t *>(_opt_info.data() + original_len) + 1) = _opt_info.length() - original_len - 4;
    }
    return;
    
  bad_opt:
    _opt_info.set_length(original_len);
}

int
ToIPFlowDumps::Flow::add_pkt(const Packet *p, ErrorHandler *errh)
{
    // ICMP errors are handled as notes, not packets
    if (PAINT_ANNO(p) >= 2) {
	assert(p->ip_header()->ip_p == IP_PROTO_ICMP);
	StringAccum sa;
	sa << p->timestamp_anno() << ' ' << (PAINT_ANNO(p) & 1 ? '>' : '<') << " ICMP_error";
	// this doesn't count as a note, really; it is a packet
	_note_count--;
	if (_packet_count < 0xFFFFFFFFU)
	    _packet_count++;
	return add_note(sa.take_string(), errh);
    }
    
    if (_npkt >= NPKT && output(errh) < 0)
	return -1;
    
    int direction = (PAINT_ANNO(p) & 1);
    const click_ip *iph = p->ip_header();
    assert(iph->ip_p == _ip_p);
    
    _pkt[_npkt].timestamp = p->timestamp_anno() - _first_timestamp;
    _pkt[_npkt].direction = direction;

    if (_ip_ids)
	_ip_ids[_npkt] = iph->ip_id;

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

	if (_tcp_opt
	    && tcph->th_off > (sizeof(click_tcp) >> 2)
	    && (tcph->th_off != 8 || *(reinterpret_cast<const uint32_t *>(tcph + 1)) != htonl(0x0101080A)))
	    store_opt(tcph, direction);

	if (_tcp_windows)
	    _tcp_windows[_npkt] = tcph->th_win;
	
    } else
	_pkt[_npkt].payload_len = ntohs(iph->ip_len) - sizeof(click_udp);
    
    _npkt++;
    if (_packet_count < 0xFFFFFFFFU)
	_packet_count++;
    
    return 0;
}

int
ToIPFlowDumps::Flow::add_note(const String &s, ErrorHandler *errh)
{
    if (_nnote >= NNOTE && output(errh) < 0)
	return -1;

    _note[_nnote].before_pkt = _npkt;
    _note[_nnote].pos = _note_text.length();
    _note_text << s;
    
    _nnote++;
    _note_count++;

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

String
ToIPFlowDumps::output_pattern() const
{
    return (_gzip ? _filename_pattern + ".gz" : _filename_pattern);
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
    bool absolute_time = false, absolute_seq = false, binary = false, tcp_opt = false, tcp_window = false, ip_id = false, gzip = false;
    _output_larger = 0;
    
    if (cp_va_parse(conf, this, errh,
		    cpOptional,
		    cpFilename, "output filename pattern", &_filename_pattern,
		    cpKeywords,
		    "OUTPUT_PATTERN", cpFilename, "output filename pattern", &_filename_pattern,
		    "NOTIFIER", cpElement, "aggregate deletion notifier", &e,
		    "ABSOLUTE_TIME", cpBool, "print absolute timestamps?", &absolute_time,
		    "ABSOLUTE_SEQ", cpBool, "print absolute sequence numbers?", &absolute_seq,
		    "BINARY", cpBool, "output binary records?", &binary,
		    "TCP_OPT", cpBool, "output TCP options?", &tcp_opt,
		    "TCP_WINDOW", cpBool, "output TCP windows?", &tcp_window,
		    "GZIP", cpBool, "gzip output files?", &gzip,
		    "IP_ID", cpBool, "output IP IDs?", &ip_id,
		    "OUTPUT_LARGER", cpUnsigned, "output flows with more than this many packets", &_output_larger,
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
    _binary = binary;
    _tcp_opt = tcp_opt;
    _tcp_window = tcp_window;
    _ip_id = ip_id;
    _gzip = gzip;

    return 0;
}

void
ToIPFlowDumps::end_flow(Flow *f, ErrorHandler *errh)
{
    if (f->npackets() > _output_larger) {
	f->output(errh);
	if (_gzip)
	    f->compress(errh);
    } else
	f->unlink(errh);
    delete f;
}

void
ToIPFlowDumps::cleanup(CleanupStage)
{
    for (int i = 0; i < NFLOWMAP; i++)
	while (Flow *f = _flowmap[i]) {
	    _flowmap[i] = f->next();
	    end_flow(f, ErrorHandler::default_handler());
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
    else if (p && (f = new Flow(p, expand_filename(p, ErrorHandler::default_handler()), _absolute_time, _absolute_seq, _binary, _ip_id, _tcp_opt, _tcp_window)))
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
ToIPFlowDumps::aggregate_notify(uint32_t agg, AggregateEvent event, const Packet *)
{
    if (event == DELETE_AGG && find_aggregate(agg, 0)) {
	_gc_aggs.push_back(agg);
	_gc_aggs.push_back(click_jiffies());
	if (!_gc_timer.scheduled())
	    _gc_timer.schedule_after_ms(250);
    }
}

void
ToIPFlowDumps::gc_hook(Timer *t, void *thunk)
{
    ToIPFlowDumps *td = static_cast<ToIPFlowDumps *>(thunk);
    uint32_t limit_jiff = click_jiffies() - (CLICK_HZ / 4);
    int i;
    for (i = 0; i < td->_gc_aggs.size() && SEQ_LEQ(td->_gc_aggs[i+1], limit_jiff); i += 2)
	if (Flow *f = td->find_aggregate(td->_gc_aggs[i], 0)) {
	    int bucket = (f->aggregate() & (NFLOWMAP - 1));
	    assert(td->_flowmap[bucket] == f);
	    td->_flowmap[bucket] = f->next();
	    td->end_flow(f, ErrorHandler::default_handler());
	}
    if (i < td->_gc_aggs.size()) {
	memmove(&td->_gc_aggs[0], &td->_gc_aggs[i], (td->_gc_aggs.size() - i) * sizeof(int));
	td->_gc_aggs.resize(td->_gc_aggs.size() - i);
	t->schedule_after_ms(250);
    }
}

enum { H_CLEAR };

int
ToIPFlowDumps::write_handler(const String &, Element *e, void *thunk, ErrorHandler *errh)
{
    ToIPFlowDumps *td = static_cast<ToIPFlowDumps *>(e);
    switch ((intptr_t)thunk) {
      case H_CLEAR:
	for (int i = 0; i < NFLOWMAP; i++)
	    while (Flow *f = td->_flowmap[i]) {
		td->_flowmap[i] = f->next();
		td->end_flow(f, errh);
	    }
	return 0;
      default:
	return -1;
    }
}

void
ToIPFlowDumps::add_handlers()
{
    add_write_handler("clear", write_handler, (void *)H_CLEAR);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ToIPFlowDumps)
