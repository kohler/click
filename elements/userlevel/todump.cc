#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "todump.hh"

#include "confparse.hh"
#include "error.hh"

#include <string.h>
#include <assert.h>

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
  : _fp(0), _timer(this)
{
}

ToDump::ToDump(String filename)
  : _filename(filename), _fp(0), _timer(this)
{
}

ToDump::~ToDump()
{
  uninitialize();
}

ToDump*
ToDump::clone() const
{
  return new ToDump(_filename);
}

int
ToDump::configure(const String &conf, ErrorHandler *errh)
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
    return errh->error("unable to open dump file");

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
  return 0;
#else
  errh->warning("dropping all packets: not compiled with pcap support");
#endif
}

void
ToDump::uninitialize()
{
  if (_fp)
    fclose(_fp);
  _fp = 0;
  _timer.unschedule();
}

bool
ToDump::wants_packet_upstream() const
{
  return true;
}

void
ToDump::run_scheduled()
{
  Packet *p = input(0).pull();
  if (!p) return;

#if HAVE_PCAP
  struct pcap_pkthdr h;
  click_gettimeofday(&h.ts);
  h.caplen = p->length();
  h.len = p->length();
  
  size_t wrote_header = fwrite(&h, sizeof(h), 1, _fp);
  assert(wrote_header == 1);
  
  size_t wrote_data = fwrite(p->data(), 1, p->length(), _fp);
  assert(wrote_data == p->length());
  
  fflush(_fp);
  _timer.schedule_after_ms(1);
#endif

  p->kill();
}

EXPORT_ELEMENT(ToDump)
