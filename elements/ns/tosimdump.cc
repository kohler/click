/*
 * tosimdump.{cc,hh} -- element writes packets to tcpdump-like file
 * Largely copied from the todump element included with the Click distribution
 *
 */

/*****************************************************************************
 *  Copyright 2002, Univerity of Colorado at Boulder.                        *
 *                                                                           *
 *                        All Rights Reserved                                *
 *                                                                           *
 *  Permission to use, copy, modify, and distribute this software and its    *
 *  documentation for any purpose other than its incorporation into a        *
 *  commercial product is hereby granted without fee, provided that the      *
 *  above copyright notice appear in all copies and that both that           *
 *  copyright notice and this permission notice appear in supporting         *
 *  documentation, and that the name of the University not be used in        *
 *  advertising or publicity pertaining to distribution of the software      *
 *  without specific, written prior permission.                              *
 *                                                                           *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS      *
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND        *
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY    *
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL         *
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA       *
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER        *
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR         *
 *  PERFORMANCE OF THIS SOFTWARE.                                            *
 *                                                                           *
 ****************************************************************************/


#include <click/config.h>
#include <click/package.hh>
#include <click/glue.hh>
#include "tosimdump.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/standard/scheduleinfo.hh>
#include <string.h>
#include <assert.h>
#include "elements/userlevel/fakepcap.hh" /* NB will fail */
#ifdef CLICK_SIM
#include <click/simclick.h>
#include <click/router.hh>
#endif
CLICK_DECLS

ToSimDump::ToSimDump()
  : Element(1, 1), _fp(0)
{
  MOD_INC_USE_COUNT;
}

ToSimDump::~ToSimDump()
{
  MOD_DEC_USE_COUNT;
  uninitialize();
}

ToSimDump*
ToSimDump::clone() const
{
  return new ToSimDump;
}

int
ToSimDump::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String encap_type;
  _snaplen = 2000;
  if (cp_va_parse(conf, this, errh,
		  cpString, "dump filename", &_filename,
		  cpOptional,
		  cpUnsigned, "max packet length", &_snaplen,
		  cpWord, "encapsulation type", &encap_type,
		  0) < 0)
    return -1;

  if (!encap_type || encap_type == "ETHER")
    _encap_type = FAKE_DLT_EN10MB;
  else if (encap_type == "IP")
    _encap_type = FAKE_DLT_RAW;
  else
    return errh->error("bad encapsulation type, expected `ETHER' or `IP'");

#ifdef CLICK_SIM
  Router* myrouter = router();
  simclick_sim mysiminst = myrouter->get_siminst();
  char tmp[255];
  simclick_sim_get_node_name(mysiminst,tmp,255);
  _filename = String(tmp) + String("_") +  _filename;
#endif

  return 0;
}

int
ToSimDump::initialize(ErrorHandler *errh)
{
  assert(!_fp);  
  _fp = fopen(_filename, "wb");
  if (!_fp)
    return errh->error("%s: %s", _filename.cc(), strerror(errno));

  struct fake_pcap_file_header h;

  h.magic = FAKE_TCPDUMP_MAGIC;
  h.version_major = FAKE_PCAP_VERSION_MAJOR;
  h.version_minor = FAKE_PCAP_VERSION_MINOR;
  
  h.thiszone = 0;		// timestamps are in GMT
  h.sigfigs = 0;		// XXX accuracy of timestamps?
  h.snaplen = _snaplen;
  h.linktype = _encap_type;
  
  size_t wrote_header = fwrite(&h, sizeof(h), 1, _fp);
  if (wrote_header != 1)
    return errh->error("unable to write to dump file");

  return 0;
}

void
ToSimDump::uninitialize()
{
  if (_fp)
    fclose(_fp);
  _fp = 0;
}

void
ToSimDump::write_packet(Packet *p)
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
  if (to_write > _snaplen)
    to_write = _snaplen;
  ph.caplen = to_write;
  ph.len = p->length();

  // XXX writing to pipe?
  if (fwrite(&ph, sizeof(ph), 1, _fp) == 0)
    click_chatter("ToSimDump(%s): %s", _filename.cc(), strerror(errno));
  else if (fwrite(p->data(), 1, to_write, _fp) == 0)
    click_chatter("ToSimDump(%s): %s", _filename.cc(), strerror(errno));
}

Packet *
ToSimDump::simple_action(Packet *p)
{
  write_packet(p);
  return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ns FakePcap)
EXPORT_ELEMENT(ToSimDump)
