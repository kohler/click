// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromdump.{cc,hh} -- element reads packets from tcpdump file
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001-2003 International Computer Science Institute
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
#include "fromdump.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/handlercall.hh>
#include <click/packet_anno.hh>
#include <click/userutils.hh>
#if CLICK_NS
# include <click/master.hh>
#endif
#include "fakepcap.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef ALLOW_MMAP
#include <sys/mman.h>
#endif
CLICK_DECLS

#define	SWAPLONG(y) \
	((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
	( (((y)&0xff)<<8) | ((u_short)((y)&0xff00)>>8) )

FromDump::FromDump()
    : _packet(0), _end_h(0), _task(this)
{
}

FromDump::~FromDump()
{
    delete _end_h;
}

void *
FromDump::cast(const char *n)
{
    if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0 && !output_is_push(0)) {
	_notifier.initialize(router());
	return static_cast<Notifier *>(&_notifier);
    } else
	return Element::cast(n);
}

String
FromDump::declaration() const
{
    StringAccum sa;
    sa << name() << " :: " << class_name();
    if (_ff.initialized())
	sa << '(' << _ff.print_filename() << ')';
    return sa.take_string();
}

int
FromDump::configure(Vector<String> &conf, ErrorHandler *errh)
{
    bool timing = false, stop = false, active = true, force_ip = false;
    Timestamp first_time, first_time_off, last_time, last_time_off, interval;
    _sampling_prob = (1 << SAMPLING_SHIFT);
#if CLICK_NS
    bool per_node = false;
#endif
    _packet_filepos = 0;

    if (_ff.configure_keywords(conf, 1, this, errh) < 0)
	return -1;
    if (cp_va_parse(conf, this, errh,
		    cpFilename, "dump file name", &_ff.filename(),
		    cpOptional,
		    cpBool, "use original packet timing?", &timing,
		    cpKeywords,
		    "TIMING", cpBool, "use original packet timing?", &timing,
		    "STOP", cpBool, "stop driver when done?", &stop,
		    "ACTIVE", cpBool, "start active?", &active,
		    "SAMPLE", cpUnsignedReal2, "sampling probability", SAMPLING_SHIFT, &_sampling_prob,
		    "FORCE_IP", cpBool, "emit IP packets only?", &force_ip,
		    "START", cpTimestamp, "starting time", &first_time,
		    "START_AFTER", cpTimestamp, "starting time offset", &first_time_off,
		    "END", cpTimestamp, "ending time", &last_time,
		    "END_AFTER", cpTimestamp, "ending time offset", &last_time_off,
		    "INTERVAL", cpTimestamp, "time interval", &interval,
		    "END_CALL", cpWriteHandlerCall, "write handler for ending time", &_end_h,
#if CLICK_NS
		    "PER_NODE", cpBool, "prepend unique node name?", &per_node,
#endif
		    "FILEPOS", cpFileOffset, "starting file position", &_packet_filepos,
		    cpEnd) < 0)
	return -1;

    // check sampling rate
    if (_sampling_prob > (1 << SAMPLING_SHIFT)) {
	errh->warning("SAMPLE probability reduced to 1");
	_sampling_prob = (1 << SAMPLING_SHIFT);
    } else if (_sampling_prob == 0)
	errh->warning("SAMPLE probability is 0; emitting no packets");

    // check times
    _have_first_time = _have_last_time = true;
    _first_time_relative = _last_time_relative = _last_time_interval = false;
    
    if ((bool) first_time + (bool) first_time_off > 1)
	return errh->error("'START' and 'START_AFTER' are mutually exclusive");
    else if (first_time)
	_first_time = first_time;
    else if (first_time_off)
	_first_time = first_time_off, _first_time_relative = true;
    else
	_have_first_time = false, _first_time_relative = true;
    
    if ((bool) last_time + (bool) last_time_off + (bool) interval > 1)
	return errh->error("'END', 'END_AFTER', and 'INTERVAL' are mutually exclusive");
    else if (last_time)
	_last_time = last_time;
    else if (last_time_off)
	_last_time = last_time_off, _last_time_relative = true;
    else if (interval)
	_last_time = interval, _last_time_interval = true;
    else
	_have_last_time = false;

    if (stop && _end_h)
	return errh->error("'END_CALL' and 'STOP' are mutually exclusive");
    else if (stop)
	_end_h = new HandlerCall(name() + ".stop");
    else if (_have_last_time && !_end_h)
	_end_h = new HandlerCall(name() + ".active false");

    // set other variables
    _have_any_times = false;
    _timing = timing;
    _force_ip = force_ip;

#ifdef CLICK_NS
    if (per_node) {
	simclick_sim mysiminst = router()->master()->siminst();
	char tmp[255];
	simclick_sim_get_node_name(mysiminst,tmp,255);
	_ff.filename() = String(tmp) + String("_") +  _ff.filename();
    }
#endif

    _active = active;
    return 0;
}

static void
swap_file_header(const fake_pcap_file_header *hp, fake_pcap_file_header *outp)
{
    outp->magic = SWAPLONG(hp->magic);
    outp->version_major = SWAPSHORT(hp->version_major);
    outp->version_minor = SWAPSHORT(hp->version_minor);
    outp->thiszone = SWAPLONG(hp->thiszone);
    outp->sigfigs = SWAPLONG(hp->sigfigs);
    outp->snaplen = SWAPLONG(hp->snaplen);
    outp->linktype = SWAPLONG(hp->linktype);
}

static void
swap_packet_header(const fake_pcap_pkthdr *hp, fake_pcap_pkthdr *outp)
{
    outp->ts.tv_sec = SWAPLONG(hp->ts.tv_sec);
    outp->ts.tv_usec = SWAPLONG(hp->ts.tv_usec);
    outp->caplen = SWAPLONG(hp->caplen);
    outp->len = SWAPLONG(hp->len);
}

FromDump *
FromDump::hotswap_element() const
{
    if (Element *e = Element::hotswap_element())
	if (FromDump *fd = static_cast<FromDump *>(e->cast("FromDump")))
	    if (fd->_ff.filename() == _ff.filename())
		return fd;
    return 0;
}

int
FromDump::initialize(ErrorHandler *errh)
{
    // make sure notifier is initialized
    if (!output_is_push(0))
	_notifier.initialize(router());
    
    // check handler call, initialize Task
    if (_end_h && _end_h->initialize_write(this, errh) < 0)
	return -1;
    if (output_is_push(0))
	ScheduleInfo::initialize_task(this, &_task, _active, errh);

    // skip if hotswapping
    if (hotswap_element())
	return 0;
    
    // open file
    if (_ff.initialize(errh) < 0)
	return -1;
    
    // check magic number
    fake_pcap_file_header swapped_fh;
    const fake_pcap_file_header *fh = (const fake_pcap_file_header *)_ff.get_aligned(sizeof(fake_pcap_file_header), &swapped_fh);
    if (!fh)
	return _ff.error(errh, "not a tcpdump file (too short)");

    if (fh->magic == FAKE_PCAP_MAGIC || fh->magic == FAKE_MODIFIED_PCAP_MAGIC)
	_swapped = false;
    else {
	swap_file_header(fh, &swapped_fh);
	_swapped = true;
	fh = &swapped_fh;
    }
    if (fh->magic != FAKE_PCAP_MAGIC && fh->magic != FAKE_MODIFIED_PCAP_MAGIC)
	return _ff.error(errh, "not a tcpdump file (bad magic number)");
    // compensate for extra crap appended to packet headers
    _extra_pkthdr_crap = (fh->magic == FAKE_PCAP_MAGIC ? 0 : sizeof(fake_modified_pcap_pkthdr) - sizeof(fake_pcap_pkthdr));

    if (fh->version_major != FAKE_PCAP_VERSION_MAJOR)
	return _ff.error(errh, "unknown major version %d", fh->version_major);
    _minor_version = fh->version_minor;
    // map possible host link types to global link types
    _linktype = fake_pcap_canonical_dlt(fh->linktype, true);

    // if forcing IP packets, check datalink type to ensure we understand it
    if (_force_ip) {
	if (!fake_pcap_dlt_force_ipable(_linktype))
	    return _ff.error(errh, "unknown linktype %d; can't force IP packets", _linktype);
	if (_timing)
	    return errh->error("FORCE_IP and TIMING options are incompatible");
    } else if (_linktype == FAKE_DLT_RAW)
	// force FORCE_IP.
	_force_ip = true;	// XXX _timing?

    // maybe skip ahead in the file
    if (_packet_filepos != 0) {
	int result = _ff.seek(_packet_filepos, errh);
	_packet_filepos = 0;
	return result;
    } else
	return 0;
}

void
FromDump::take_state(Element *e, ErrorHandler *errh)
{
    FromDump *o = static_cast<FromDump *>(e); // checked by hotswap_element()

    _ff.take_state(o->_ff, errh);

    _packet = o->_packet;
    o->_packet = 0;

    _swapped = o->_swapped;
    _extra_pkthdr_crap = o->_extra_pkthdr_crap;
    _minor_version = o->_minor_version;
    
    _linktype = o->_linktype;
    if (_linktype == FAKE_DLT_RAW)
	_force_ip = true;
    else if (_force_ip && !fake_pcap_dlt_force_ipable(_linktype))
	_ff.warning(errh, "unknown linktype %d; can't force IP packets", _linktype);

    _time_offset = o->_time_offset;
    _packet_filepos = o->_packet_filepos;
}

void
FromDump::cleanup(CleanupStage)
{
    _ff.cleanup();
    if (_packet)
	_packet->kill();
    _packet = 0;
}

void
FromDump::set_active(bool active)
{
    _active = active;
    if (active) {
	if (output_is_push(0) && !_task.scheduled())
	    _task.reschedule();
	else if (!output_is_push(0))
	    _notifier.wake();
    }
}

void
FromDump::prepare_times(const Timestamp &ts)
{
    if (_first_time_relative)
	_first_time += ts;
    if (_last_time_relative)
	_last_time += ts;
    else if (_last_time_interval)
	_last_time += _first_time;
    if (_timing)
	_time_offset = Timestamp::now() - ts;
    _have_any_times = true;
}

bool
FromDump::read_packet(ErrorHandler *errh)
{
    fake_pcap_pkthdr swapped_ph;
    const fake_pcap_pkthdr *ph;
    const Timestamp *ts_ptr;
    int len, caplen, skiplen = 0;
    Packet *p;
    int tries = 0;
    assert(!_packet);

  retry:
    // quit if we have tried too many times
    tries++;
    if ((tries % 16) == 0 && output_is_push(0))
	return true;

    // record file position
    _packet_filepos = _ff.file_pos();

    // read the packet header
    if (!(ph = reinterpret_cast<const fake_pcap_pkthdr *>(_ff.get_aligned(sizeof(*ph), &swapped_ph))))
	return false;
    if (_swapped) {
	swap_packet_header(ph, &swapped_ph);
	ph = &swapped_ph;
    }

    // may need to swap 'caplen' and 'len' fields at or before version 2.3
    if (_minor_version > 3 || (_minor_version == 3 && ph->caplen <= ph->len)) {
	len = ph->len;
	caplen = ph->caplen;
    } else {
	len = ph->caplen;
	caplen = ph->len;
    }

    // check for errors
    // 3.Jul.2002 -- Angelos Stavrou discovered that tcptrace-generated
    // tcpdump files store an incorrect caplen. It's only off by one. Tcptrace
    // should be fixed, but we hack around the problem here, as does
    // tcpdump itself.
    if (caplen > 65535) {
	_ff.error(errh, "bad packet header; giving up");
	return false;
    } else if (caplen > len) {
	skiplen = caplen - len;
	caplen = len;
    }

    // compensate for modified pcap versions
    _ff.shift_pos(_extra_pkthdr_crap);

    // check times
  check_times:
    ts_ptr = fake_bpf_timeval::make_timestamp(&ph->ts, &swapped_ph.ts);
    if (!_have_any_times)
	prepare_times(*ts_ptr);
    if (_have_first_time) {
	if (*ts_ptr < _first_time) {
	    _ff.shift_pos(caplen + skiplen);
	    goto retry;
	} else
	    _have_first_time = false;
    }
    if (_have_last_time && *ts_ptr >= _last_time) {
	_have_last_time = false;
	(void) _end_h->call_write(errh);
	if (!_active) {
	    _ff.shift_pos(caplen + skiplen);
	    return false;
	}
	// retry _last_time in case someone changed it
	goto check_times;
    }
    
    // checking sampling probability
    if (_sampling_prob < (1 << SAMPLING_SHIFT)
	&& (uint32_t)(random() & ((1<<SAMPLING_SHIFT)-1)) >= _sampling_prob) {
	_ff.shift_pos(caplen + skiplen);
	goto retry;
    }
    
    // create packet
    p = _ff.get_packet(caplen, ts_ptr->sec(), ts_ptr->subsec(), errh);
    if (!p)
	return false;
    SET_EXTRA_LENGTH_ANNO(p, len - caplen);
    _ff.shift_pos(skiplen);

    p->set_mac_header(p->data());
    
    if (_force_ip && !fake_pcap_force_ip(p, _linktype)) {
	checked_output_push(1, p);
	goto retry;
    }

    _packet = p;
    return true;
}

bool
FromDump::run_task(Task *)
{
    if (!_active)
	return false;

    bool more = true;
    if (!_packet)
	more = read_packet(0);
    if (_packet && _timing)
	if (_packet->timestamp_anno() > Timestamp::now() - _time_offset) {
	    _task.fast_reschedule();
	    return false;
	}

    if (more)
	_task.fast_reschedule();
    else if (_end_h)
	_end_h->call_write(ErrorHandler::default_handler());

    if (_packet) {
	output(0).push(_packet);
	_packet = 0;
	return true;
    } else
	return false;
}

Packet *
FromDump::pull(int)
{
    if (!_active) {
	_notifier.sleep();
	return 0;
    }

    bool more = true;
    if (!_packet)
	more = read_packet(0);
    if (_packet && _timing)
	if (_packet->timestamp_anno() > Timestamp::now() - _time_offset)
	    return 0;

    // notify presence/absence of more packets
    _notifier.set_active(more, true);
    if (!more && _end_h)
	_end_h->call_write(ErrorHandler::default_handler());
    
    Packet *p = _packet;
    _packet = 0;
    return p;
}

enum {
    H_SAMPLING_PROB, H_ACTIVE, H_ENCAP, H_STOP, H_PACKET_FILEPOS,
    H_EXTEND_INTERVAL
};

String
FromDump::read_handler(Element *e, void *thunk)
{
    FromDump *fd = static_cast<FromDump *>(e);
    switch ((intptr_t)thunk) {
    case H_SAMPLING_PROB:
	return cp_unparse_real2(fd->_sampling_prob, SAMPLING_SHIFT);
    case H_ACTIVE:
	return cp_unparse_bool(fd->_active);
    case H_ENCAP:
	return String(fake_pcap_unparse_dlt(fd->_linktype));
    case H_PACKET_FILEPOS:
	return String(fd->_packet_filepos);
    default:
	return "<error>";
    }
}

int
FromDump::write_handler(const String &s_in, Element *e, void *thunk, ErrorHandler *errh)
{
    FromDump *fd = static_cast<FromDump *>(e);
    String s = cp_uncomment(s_in);
    switch ((intptr_t)thunk) {
      case H_ACTIVE: {
	  bool active;
	  if (cp_bool(s, &active)) {
	      fd->set_active(active);
	      return 0;
	  } else
	      return errh->error("'active' should be Boolean");
      }
      case H_STOP:
	fd->set_active(false);
	fd->router()->please_stop_driver();
	return 0;
      case H_EXTEND_INTERVAL: {
	  Timestamp ts;
	  if (cp_time(s, &ts)) {
	      fd->_last_time += ts;
	      if (fd->_end_h)
		  fd->_have_last_time = true, fd->set_active(true);
	      return 0;
	  } else
	      return errh->error("'extend_interval' takes a time interval");
      }
      default:
	return -EINVAL;
    }
}

void
FromDump::add_handlers()
{
    _ff.add_handlers(this, true);
    add_read_handler("sampling_prob", read_handler, (void *)H_SAMPLING_PROB);
    add_read_handler("active", read_handler, (void *)H_ACTIVE);
    add_write_handler("active", write_handler, (void *)H_ACTIVE);
    add_read_handler("encap", read_handler, (void *)H_ENCAP);
    add_write_handler("stop", write_handler, (void *)H_STOP);
    add_read_handler("packet_filepos", read_handler, (void *)H_PACKET_FILEPOS);
    add_write_handler("extend_interval", write_handler, (void *)H_EXTEND_INTERVAL);
    if (output_is_push(0))
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakePcap FromFile)
EXPORT_ELEMENT(FromDump)
