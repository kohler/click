#ifndef TOBPF_HH
#define TOBPF_HH
#include "element.hh"
#include "string.hh"

/*
 * =c
 * ToBPF(devname)
 * =d
 * Pulls packets and sends them out the named device using
 * the Berkeley Packet Filter (or Linux equivalent).
 *
 * Incoming packets should already have the link-level
 * header prepended. This means that ARP processing,
 * for example, must already have been done.
 *
 * Under Linux, tries to re-use the file descriptor of a
 * previously declared FromBPF, so that the latter
 * won't see packets sent by us.
 *
 * This element is only available at user level.
 * 
 * =a FromBPF
 * =a FromDump
 * =a ToDump
 */

#ifdef HAVE_PCAP
extern "C" {
# include <pcap.h>
}
#else
# include "fakepcap.h"
#endif

/*
 * Write packets to the ethernet via the bpf.
 * Expects packets that already have an ether header.
 * Can push or pull.
 */

class ToBPF : public Element {

  String _ifname;
  int _fd; /* might be borrowed from FromBPF */
  pcap_t * _pcap;

 public:
  
  ToBPF();
  ToBPF(const String &ifname);
  ~ToBPF();
  
  const char *class_name() const		{ return "ToBPF"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  ToBPF *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  void push(int port, Packet *);

  void run_scheduled();
  
};

#endif
