/*
 * todump.{cc,hh} -- element writes packets to tcpdump-like file
 * via pcap library
 * John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "glue.hh"
#include "todump.hh"
#include "confparse.hh"
#include "error.hh"
#include "elements/standard/scheduleinfo.hh"
#include <string.h>
#include <assert.h>
#include <errno.h>

#ifdef HAVE_PCAP
extern "C" {
# include <pcap.h>
}
#else
# include "fakepcap.h"
#endif

#ifndef TCPDUMP_MAGIC
#define TCPDUMP_MAGIC 0xa1b2c3d4
#endif

ToDump::ToDump()
  : _fp(0)
{
  add_input();
}

ToDump::~ToDump()
{
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
  return cp_va_parse(conf, this, errh,
		     cpString, "dump filename", &_filename,
		     0);
}

int
ToDump::initialize(ErrorHandler *errh)
{
  assert(!_fp);  
  _fp = fopen(_filename, "wb");
  if (!_fp)
    return errh->error("%s: %s", _filename.cc(), strerror(errno));

#ifdef HAVE_PCAP
  struct pcap_file_header h;
  memset(&h, '\0', sizeof(h));
  h.magic = TCPDUMP_MAGIC;
  h.version_major = PCAP_VERSION_MAJOR;
  h.version_minor = PCAP_VERSION_MINOR;
  h.thiszone = 0; /* XXX */
  h.sigfigs = 0;  /* XXX */
  h.snaplen = 20000; /* XXX */
  h.linktype = 1; /* DLT_EN10MB; */ /* XXX */
  size_t wrote_header = fwrite(&h, sizeof(h), 1, _fp);
  if (wrote_header != 1)
    return errh->error("unable to write to dump file");
#else
  errh->warning("dropping all packets: not compiled with pcap support");
#endif

  if (input_is_pull(0))
    ScheduleInfo::join_scheduler(this, errh);
  return 0;
}

void
ToDump::uninitialize()
{
  if (_fp)
    fclose(_fp);
  _fp = 0;
}

void
ToDump::write_packet(Packet *p)
{
#if HAVE_PCAP
  struct timeval now;
  click_gettimeofday(&now);
  
  struct pcap_pkthdr h;
  h.ts.tv_sec = now.tv_sec;
  h.ts.tv_usec = now.tv_usec;
  h.caplen = p->length();
  h.len = p->length();
  
  size_t wrote_header = fwrite(&h, sizeof(h), 1, _fp);
  assert(wrote_header == 1);
  
  size_t wrote_data = fwrite(p->data(), 1, p->length(), _fp);
  assert(wrote_data == p->length());
  
  // fflush(_fp);
#endif
}

void
ToDump::push(int, Packet *p)
{
  write_packet(p);
  p->kill();
}

void
ToDump::run_scheduled()
{
  Packet *p = input(0).pull();
  if (p) {
    write_packet(p);
    p->kill();
  }
  reschedule();
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(ToDump)
