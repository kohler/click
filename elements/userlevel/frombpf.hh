#ifndef FROMBPF_HH
#define FROMBPF_HH

/*
 * =c
 * FromBPF(DEVNAME, PROMISC?)
 * =d
 * Reads ethernet packets from the Berkeley Packet Filter
 * (or Linux equivalent), from the network interface
 * device named DEVNAME.
 * Puts the device in promiscuous mode if PROMISC? (a Boolean) is true.
 *
 * The kernel networking code sees all of the packets that
 * FromBPF produces; be careful that at most one of Click
 * and the kernel forwards each packet.
 *
 * Under Linux, a FromBPF element will not receive packets sent by a ToBPF
 * element for the same device. Under other operating systems, your mileage
 * may vary.
 *
 * This element is only available at user level.
 * 
 * =e
 * = FromBPF(eth0, 0) -> ...
 *
 * =a ToBPF
 * =a FromDump
 * =a ToDump
 */

#include "element.hh"

#ifdef HAVE_PCAP
extern "C" {
# include <pcap.h>
}
#else
# include "fakepcap.h"
#endif

class FromBPF : public Element {
 public:
  
  FromBPF();
  ~FromBPF();
  
  const char *class_name() const		{ return "FromBPF"; }
  const char *processing() const		{ return PUSH; }
  
  FromBPF *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void run_scheduled();

  // For LiveWriter, so it can borrow our _pcap.
  String get_ifname() const	{ return _ifname; }
  pcap_t *get_pcap() const	{ return _pcap; }  

 private:
  String _ifname;
  bool _promisc;
  pcap_t* _pcap;

#ifdef HAVE_PCAP
  static void get_packet(u_char* clientdata,
			 const struct pcap_pkthdr* pkthdr,
			 const u_char* data);
  int do_select(int waitms);
#endif

};

#endif
