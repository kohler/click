/* -*- c-basic-offset: 4 -*- */

#include <click/config.h>

#include "fromtusummarylog.hh"
#include "elements/standard/scheduleinfo.hh"
#include <click/error.hh>
#include <click/router.hh>
#include <click/click_ip.h>
#include <click/click_udp.h>
#include <click/click_tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

FromTUSummaryLog::FromTUSummaryLog()
    : Element(0, 1), _fd(-1), _buf(0), _pos(0), _len(0), _cap(0), _task(this)
{
    MOD_INC_USE_COUNT;
}

FromTUSummaryLog::~FromTUSummaryLog()
{
    MOD_DEC_USE_COUNT;
}

int
FromTUSummaryLog::configure(const Vector<String> &conf, ErrorHandler *errh)
{
    _active = true;
    _stop = false;
    if (cp_va_parse(conf, this, errh,
		    cpString, "log filename", &_filename,
		    cpKeywords,
		    "ACTIVE", cpBool, "active?", &_active,
		    "STOP", cpBool, "stop when done?", &_stop,
		    0) < 0)
	return -1;
    if (_filename == "")
	return errh->error("empty filename");
    return 0;
}

int
FromTUSummaryLog::initialize(ErrorHandler *errh)
{
    if (_filename == "-")
	_fd = STDIN_FILENO;
    else {
	_fd = open(_filename.cc(), O_RDONLY);
	if (_fd < 0)
	    return errh->error("%s: %s", _filename.cc(), strerror(errno));
    }

    if (output_is_push(0))
	ScheduleInfo::join_scheduler(this, &_task, errh);

    return 0;
}

void
FromTUSummaryLog::uninitialize()
{
    if (_fd >= 0 && _fd != STDIN_FILENO)
	close(_fd);
    delete[] _buf;
    _buf = 0;
    _task.unschedule();
}

bool
FromTUSummaryLog::read_more_buf()
{
    // first, shift down unused portion of buffer
    if (_cap - _pos < 1024 && _cap >= 2048) {
	// can use memcpy because _cap >= 2048
	memcpy(_buf, _buf + _pos, _len - _pos);
	_len -= _pos;
	_pos = 0;
    }

    // also might grow buffer
    if (_len == _cap) {
	int new_cap = (_cap ? _cap * 2 : 2048);
	char *new_buf = new char[new_cap];
	if (!new_buf)
	    return false;
	memcpy(new_buf, _buf, _cap);
	delete[] _buf;
	_buf = new_buf;
	_cap = new_cap;
    }

    int old_len = _len;
    
    // finally, read some data
    while (_len < _cap) {
	int r = read(_fd, _buf + _len, _cap - _len);
	if (r < 0 && errno != EINTR) {
	    click_chatter("%s: %s", declaration().cc(), strerror(errno));
	    break;
	} else if (r == 0)	// EOF
	    break;
	else
	    _len += r;
    }

    return (_len > old_len);
}

Packet *
FromTUSummaryLog::try_read_packet()
{
    // first, get line
    int line = _pos;
    while (1) {
	while (line < _len && _buf[line] != '\r' && _buf[line] != '\n')
	    line++;
	if (line == _len) {
	    line -= _pos;
	    if (!read_more_buf()) {
		_active = false;
		return 0;
	    }
	    line += _pos;
	} else
	    break;
    }
    // fiddle with line ending
    _buf[line] = 0;

    int ts, tus, src[4], sport, dst[4], dport, len;
    char t;

    if (sscanf(_buf + _pos, " %u.%u %u.%u.%u.%u %u %u.%u.%u.%u %u %c %u ",
	       &ts, &tus, &src[0], &src[1], &src[2], &src[3], &sport,
	       &dst[0], &dst[1], &dst[2], &dst[3], &dport, &t, &len)
	!= 14) {
	_pos = line + 1;
	return 0;
    }

    if (src[0] > 255 || src[1] > 255 || src[2] > 255 || src[3] > 255
	|| dst[0] > 255 || dst[1] > 255 || dst[2] > 255 || dst[3] > 255
	|| sport > 65535 || dport > 65535 || len > 2048
	|| (t != 'T' && t != 'U')) {
	_pos = line + 1;
	return 0;
    }
    
    // create packet
    WritablePacket *q = Packet::make(14, 0, (t == 'T' ? 40 + len : 28 + len), 14);
    if (!q) {			// out of memory
	_active = false;
	return 0;
    }

    q->set_timestamp_anno(ts, tus);
    q->set_network_header(q->data(), 20);
    click_ip *ip = q->ip_header();

    ip->ip_v = 4;
    ip->ip_hl = 5;		// == 20
    ip->ip_len = htons(q->length());
    ip->ip_src.s_addr = htonl((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
    ip->ip_dst.s_addr = htonl((dst[0] << 24) | (dst[1] << 16) | (dst[2] << 8) | dst[3]);

    if (t == 'T') {
	ip->ip_p = IPPROTO_TCP;
	click_tcp *tcp = (click_tcp *)(ip + 1);
	tcp->th_sport = htons(sport);
	tcp->th_dport = htons(dport);
	tcp->th_flags = 0;
    } else {
	ip->ip_p = IPPROTO_UDP;
	click_udp *udp = (click_udp *)(ip + 1);
	udp->uh_sport = htons(sport);
	udp->uh_dport = htons(dport);
	udp->uh_ulen = htons(len);
	udp->uh_sum = 0;
    }

    _pos = line + 1;
    return q;
}

Packet *
FromTUSummaryLog::read_packet()
{
    Packet *p;
    while (!(p = try_read_packet()) && _active)
	/* nada */;
    return p;
}

void
FromTUSummaryLog::run_scheduled()
{
    if (_active) {
	if (Packet *p = read_packet())
	    output(0).push(p);
	_task.fast_reschedule();
    } else {
	if (_stop)
	    router()->please_stop_driver();
    }
}

Packet *
FromTUSummaryLog::pull(int)
{
    if (_active)
	return read_packet();
    else {
	if (_stop)
	    router()->please_stop_driver();
	return 0;
    }
}

void
FromTUSummaryLog::add_handlers()
{
    if (output_is_push(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromTUSummaryLog)
