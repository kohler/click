/*
 * frombpf.{cc,hh} -- element reads packets live from network via pcap
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
#include "frombpf.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "../standard/scheduleinfo.hh"
#include "glue.hh"
#include <unistd.h>

FromBPF::FromBPF()
  : _promisc(0), _pcap(0)
{
  add_output();
}

FromBPF::~FromBPF()
{
  if (_pcap) pcap_close(_pcap);
}

FromBPF *
FromBPF::clone() const
{
  return new FromBPF;
}

int
FromBPF::configure(const String &conf, ErrorHandler *errh)
{
  if (_pcap) pcap_close(_pcap);
  _pcap = 0;

  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_ifname,
		  cpOptional,
		  cpBool, "be promiscuous", &_promisc,
		  cpEnd) < 0)
    return -1;

  return 0;
}

int
FromBPF::initialize(ErrorHandler *errh)
{
  if (_pcap)
    return 0;
  else if (!_ifname)
    return errh->error("interface not set");
  
  /* 
   * Later versions of pcap distributed with linux (e.g. the redhat
   * linux pcap-0.4-16) want to have a filter installed before they
   * will pick up any packets.
   */

#ifdef HAVE_PCAP
  char *ifname = _ifname.mutable_c_str();
  char ebuf[PCAP_ERRBUF_SIZE];
  _pcap = pcap_open_live(ifname,
                         12000, /* XXX snaplen */
                         _promisc,
                         1,     /* don't batch packets */
                         ebuf);
  if (!_pcap)
    return errh->error("%s: %s", ifname, ebuf);

  bpf_u_int32 netmask;
  bpf_u_int32 localnet;
  if (pcap_lookupnet(ifname, &localnet, &netmask, ebuf) < 0) {
    errh->warning("%s: %s", ifname, ebuf);
  }
  
  struct bpf_program fcode;
  /* 
   * assume we can use 0 pointer for program string and get ``empty''
   * filter program.  
   */
  if (pcap_compile(_pcap, &fcode, 0, 0, netmask) < 0) {
    return errh->error("%s: %s", ifname, pcap_geterr(_pcap));
  }

  if (pcap_setfilter(_pcap, &fcode) < 0) {
    return errh->error("%s: %s", ifname, pcap_geterr(_pcap));
  }


#else
  errh->warning("can't get packets: not compiled with pcap support");
#endif

#ifndef RR_SCHED
  int max_tickets = ScheduleInfo::query(this,errh);
  set_max_tickets(max_tickets);
  set_tickets(max_tickets);
#endif
  join_scheduler();
  
  return 0;
}

#ifdef HAVE_PCAP
void
FromBPF::get_packet(u_char* clientdata,
		    const struct pcap_pkthdr* pkthdr,
		    const u_char* data) {
  FromBPF *lr = (FromBPF *) clientdata;
  int length = pkthdr->caplen;
  Packet* p = Packet::make(data, length);
  lr->output(0).push(p);
}
#endif

void
FromBPF::selected(int)
{
  /*
   * Read and push() one buffer of packets.
   */
  pcap_dispatch(_pcap, -1, FromBPF::get_packet, (u_char *) this);
}

void
FromBPF::run_scheduled()
{
  selected(0);
  reschedule();
}


EXPORT_ELEMENT(FromBPF)
