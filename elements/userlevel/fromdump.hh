#ifndef FROMDUMP_HH
#define FROMDUMP_HH

/*
 * =c
 * FromDump(FILENAME [, TIMING])
 * =d
 *
 * Reads packets from a file produced by `tcpdump -w FILENAME' or ToDump.
 * Pushes them out the output, and stops the driver when there are no more
 * packets. If TIMING is true, then FromDump tries to maintain the timing of
 * the original packet stream. TIMING is true by default.
 *
 * By default, `tcpdump -w FILENAME' dumps only the first 68 bytes of
 * each packet. You probably want to run `tcpdump -w FILENAME -s 2000' or some
 * such.
 *
 * Only available in user-level processes.
 *
 * =a ToDump
 * =a FromBPF
 * =a ToBPF */

#ifdef HAVE_PCAP
extern "C" {
# include <pcap.h>
}
#else
# include "fakepcap.h"
#endif
#include <sys/types.h>
#include "element.hh"

class FromDump : public Element {
  
  pcap_t* _pcap;
#ifdef HAVE_PCAP
  pcap_pkthdr _pending_pkthdr;
#endif
  Packet* _pending_packet;
  bpf_timeval _bpf_offset;
  bpf_timeval _bpf_init;
  String _filename;
  bool _timing;

#ifdef HAVE_PCAP
  static void pcap_packet_hook(u_char* clientdata,
			       const struct pcap_pkthdr* pkthdr,
			       const u_char* data);
#endif
  
 public:
  
  FromDump();
  ~FromDump();

  const char *class_name() const		{ return "FromDump"; }
  const char *processing() const		{ return PUSH; }
  FromDump *clone() const;
  
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void run_scheduled();
  
};

#endif
