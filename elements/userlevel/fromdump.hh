#ifndef FROMDUMP_HH
#define FROMDUMP_HH

/*
 * =c
 * FromDump(filename)
 * =d
 * Reads packets from a file produced by tcpdump -w.
 * Pushes them out the output.
 * Tries to maintain the timing of the original packet stream.
 *
 * Only available in user-level processes.
 *
 * =a ToDump
 * =a FromBPF
 * =a ToBPF
 */

extern "C" {
#include <pcap.h>
}
#include <sys/types.h>

#include "element.hh"

class FromDump : public Element {
 public:
  FromDump();
  FromDump(String filename);
  ~FromDump();

  bool ready();
  void go();
  
  const char *class_name() const		{ return "FromDump"; }
  Processing default_processing() const	{ return PUSH; }
  FromDump *clone() const;
  
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);

 private:
  pcap_t* _pcap;
  pcap_pkthdr _pending_pkthdr;
  Packet* _pending_packet;
  timeval _offset;
  String _filename;

  // static get_packet just casts clientdata to a FromDump and calls
  // the method. (used only as a callback)
  static void get_packet(u_char* clientdata,
			 const struct pcap_pkthdr* pkthdr,
			 const u_char* data);
  void get_packet(const pcap_pkthdr* pkthdr, const u_char* data);
  

};

#endif
