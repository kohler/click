#ifndef FROMBPF_HH
#define FROMPPF_HH

/*
 * =c
 * FromBPF(devname, promisc?)
 * =d
 * Reads ethernet packets from the Berkeley Packet Filter
 * (or Linux equivalent), from the network interface
 * device named devname.
 * Puts the device in promiscuous mode if the 2nd configuration
 * argument is 1.
 *
 * If a ToBPF is defined with the same device name
 * subsequently, that ToBPF will share a file descriptor
 * with the FromBPF. Under Linux that causes the FromBPF
 * to not see packets sent by the ToBPF.
 *
 * The kernel networking code sees all of the packets that
 * FromBPF produces; be careful that at most one of Click
 * and the kernel forwards each packet.
 *
 * This element is only available at user level.
 * 
 * =e
 * = FromBPF(eth0, 0)
 *
 * =a ToBPF
 * =a FromDump
 * =a ToDump
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "element.hh"
#include "string.hh"

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
  Processing default_processing() const		{ return PUSH; }
  
  FromBPF *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  int select_fd()		{ return (_pcap?pcap_fileno(_pcap):-1); }
  void selected(int);
  
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
