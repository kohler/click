/*
 * fromdump.{cc,hh} -- element reads packets from tcpdump file
 * John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>
#include <click/package.hh>
#include "fromdump.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include "elements/standard/scheduleinfo.hh"
#include <click/error.hh>
#include <click/glue.hh>
#include <click/click_ip.h>
#include "fakepcap.h"

#define	SWAPLONG(y) \
	((((y)&0xff)<<24) | (((y)&0xff00)<<8) | (((y)&0xff0000)>>8) | (((y)>>24)&0xff))
#define	SWAPSHORT(y) \
	( (((y)&0xff)<<8) | ((u_short)((y)&0xff00)>>8) )

FromDump::FromDump()
  : Element(0, 1), _fp(0), _packet(0), _task(this)
{
  MOD_INC_USE_COUNT;
}

FromDump::~FromDump()
{
  MOD_DEC_USE_COUNT;
  assert(!_fp && !_packet);
}

int
FromDump::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _timing = true;
  return cp_va_parse(conf, this, errh,
		     cpString, "dump file name", &_filename,
		     cpOptional,
		     cpBool, "use original packet timing", &_timing,
		     0);
}

static void
swap_file_header(fake_pcap_file_header *hp)
{
  hp->magic = SWAPLONG(hp->magic);
  hp->version_major = SWAPSHORT(hp->version_major);
  hp->version_minor = SWAPSHORT(hp->version_minor);
  hp->thiszone = SWAPLONG(hp->thiszone);
  hp->sigfigs = SWAPLONG(hp->sigfigs);
  hp->snaplen = SWAPLONG(hp->snaplen);
  hp->linktype = SWAPLONG(hp->linktype);
}

static void
swap_packet_header(fake_pcap_pkthdr *hp)
{
  hp->ts.tv_sec = SWAPLONG(hp->ts.tv_sec);
  hp->ts.tv_usec = SWAPLONG(hp->ts.tv_usec);
  hp->caplen = SWAPLONG(hp->caplen);
  hp->len = SWAPLONG(hp->len);
}

int
FromDump::initialize(ErrorHandler *errh)
{
  _fp = fopen(_filename, "rb");
  if (!_fp)
    return errh->error("%s: %s", _filename.cc(), strerror(errno));

  fake_pcap_file_header fh;
  if (fread(&fh, sizeof(fh), 1, _fp) < 1)
    return errh->error("%s: not a tcpdump file (too short)", _filename.cc());

  if (fh.magic == FAKE_TCPDUMP_MAGIC)
    _swapped = false;
  else {
    swap_file_header(&fh);
    _swapped = true;
  }
  if (fh.magic != FAKE_TCPDUMP_MAGIC)
    return errh->error("%s: not a tcpdump file (bad magic number)", _filename.cc());

  if (fh.version_major != FAKE_PCAP_VERSION_MAJOR)
    return errh->error("%s: unknown major version %d", _filename.cc(), fh.version_major);
  _minor_version = fh.version_minor;
  _linktype = fh.linktype;

  _packet = read_packet(errh);
  if (_packet) {
    struct timeval now;
    click_gettimeofday(&now);
    timersub(&now, &_packet->timestamp_anno(), &_time_offset);

    ScheduleInfo::join_scheduler(this, &_task, errh);
  } else
    errh->warning("%s: no packets", _filename.cc());
  
  return 0;
}

void
FromDump::uninitialize()
{
  if (_fp) {
    fclose(_fp);
    _fp = 0;
  }
  if (_packet) {
    _packet->kill();
    _packet = 0;
  }
  _task.unschedule();
}

WritablePacket *
FromDump::read_packet(ErrorHandler *errh)
{
  fake_pcap_pkthdr ph;
  if (fread(&ph, sizeof(fake_pcap_pkthdr), 1, _fp) < 1)
    return 0;

  if (_swapped)
    swap_packet_header(&ph);

  // may need to swap 'caplen' and 'len' fields at or before version 2.3
  if (_minor_version < 3 || (_minor_version == 3 && ph.caplen > ph.len)) {
    int t = ph.caplen;
    ph.caplen = ph.len;
    ph.len = t;
  }

  // check for errors
  if (ph.caplen > ph.len || ph.caplen > 65535) {
    if (errh)
      errh->error("%s: bad packet header; giving up", _filename.cc());
    else
      click_chatter("FromDump(%s): bad packet header; giving up", _filename.cc());
    return 0;
  }

  // make packet
  WritablePacket *wp = Packet::make(0, 0, ph.caplen, 0);
  if (!wp) {
    click_chatter("FromDump(%s): out of memory", _filename.cc());
    return 0;
  }

  size_t r = fread(wp->data(), 1, ph.caplen, _fp);
  if (r < ph.caplen) {
    click_chatter("FromDump(%s): short packet", _filename.cc());
    wp->kill();
    return 0;
  }

  wp->set_timestamp_anno(ph.ts.tv_sec, ph.ts.tv_usec);

  if (_linktype == FAKE_DLT_RAW && ph.caplen > 20) {
    click_ip *iph = reinterpret_cast<click_ip *>(wp->data());
    wp->set_ip_header(iph, iph->ip_hl << 2);
  }
  
  return wp;
}

void
FromDump::run_scheduled()
{
  if (_timing) {
    struct timeval now;
    click_gettimeofday(&now);
    timersub(&now, &_time_offset, &now);
    if (timercmp(&_packet->timestamp_anno(), &now, >)) {
      _task.fast_reschedule();
      return;
    }
  }

  output(0).push(_packet);
  _packet = read_packet(0);
  if (_packet)
    _task.fast_reschedule();
}

void
FromDump::add_handlers()
{
  add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromDump)
