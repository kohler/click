/*
 * todump.{cc,hh} -- element writes packets to tcpdump-like file
 * John Jannotti, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <string.h>
#include <assert.h>
#include "fakepcap.h"

ToDump::ToDump()
  : Element(1, 0), _fp(0), _task(this)
{
  MOD_INC_USE_COUNT;
}

ToDump::~ToDump()
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}

ToDump*
ToDump::clone() const
{
  return new ToDump;
}

int
ToDump::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String encap_type;
  _snaplen = 2000;
  _extra_length = true;
  
  if (cp_va_parse(conf, this, errh,
		  cpFilename, "dump filename", &_filename,
		  cpOptional,
		  cpUnsigned, "max packet length", &_snaplen,
		  cpWord, "encapsulation type", &encap_type,
		  cpKeywords,
		  "SNAPLEN", cpUnsigned, "max packet length", &_snaplen,
		  "ENCAP", cpWord, "encapsulation type", &encap_type,
		  "EXTRA_LENGTH", cpBool, "record extra length?", &_extra_length,
		  0) < 0)
    return -1;

  encap_type = encap_type.upper();
  if (!encap_type || encap_type == "ETHER")
    _encap_type = FAKE_DLT_EN10MB;
  else if (encap_type == "IP")
    _encap_type = FAKE_DLT_RAW;
  else
    return errh->error("bad encapsulation type, expected `ETHER' or `IP'");

  return 0;
}

int
ToDump::initialize(ErrorHandler *errh)
{
  assert(!_fp);
  if (_filename != "-") {
      _fp = fopen(_filename, "wb");
      if (!_fp)
	  return errh->error("%s: %s", _filename.cc(), strerror(errno));
  } else {
      _fp = stdout;
      _filename = "<stdout>";
  }

  struct fake_pcap_file_header h;

  h.magic = FAKE_PCAP_MAGIC;
  h.version_major = FAKE_PCAP_VERSION_MAJOR;
  h.version_minor = FAKE_PCAP_VERSION_MINOR;
  
  h.thiszone = 0;		// timestamps are in GMT
  h.sigfigs = 0;		// XXX accuracy of timestamps?
  h.snaplen = _snaplen;
  h.linktype = _encap_type;
  
  size_t wrote_header = fwrite(&h, sizeof(h), 1, _fp);
  if (wrote_header != 1)
    return errh->error("%s: unable to write file header", _filename.cc());

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, &_task, errh);
  _active = true;
  return 0;
}

void
ToDump::uninitialize()
{
  if (_fp && _fp != stdout)
    fclose(_fp);
  _fp = 0;
  _task.unschedule();
}

void
ToDump::write_packet(Packet *p)
{
  struct fake_pcap_pkthdr ph;
  
  const struct timeval &ts = p->timestamp_anno();
  if (!ts.tv_sec && !ts.tv_usec) {
    struct timeval now;
    click_gettimeofday(&now);
    ph.ts.tv_sec = now.tv_sec;
    ph.ts.tv_usec = now.tv_usec;
  } else {
    ph.ts.tv_sec = ts.tv_sec;
    ph.ts.tv_usec = ts.tv_usec;
  }

  unsigned to_write = p->length();
  ph.len = to_write + (_extra_length ? EXTRA_LENGTH_ANNO(p) : 0);
  if (to_write > _snaplen)
    to_write = _snaplen;
  ph.caplen = to_write;

  // XXX writing to pipe?
  if (fwrite(&ph, sizeof(ph), 1, _fp) == 0
      || fwrite(p->data(), 1, to_write, _fp) == 0) {
      if (errno != EAGAIN) {
	  _active = false;
	  click_chatter("ToDump(%s): %s", _filename.cc(), strerror(errno));
      }
  }
}

void
ToDump::push(int, Packet *p)
{
    if (_active)
	write_packet(p);
    p->kill();
}

void
ToDump::run_scheduled()
{
    if (!_active)
	return;
    Packet *p = input(0).pull();
    if (p) {
	write_packet(p);
	p->kill();
    }
    _task.fast_reschedule();
}

void
ToDump::add_handlers()
{
    if (input_is_pull(0))
	add_task_handlers(&_task);
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ToDump)
