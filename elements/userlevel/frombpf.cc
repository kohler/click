#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "frombpf.hh"
#include "error.hh"
#include "packet.hh"
#include "confparse.hh"
#include "glue.hh"
#include <unistd.h>

FromBPF::FromBPF()
  : _promisc(0), _pcap(0)
{
  add_output();
}

FromBPF::FromBPF(const String &ifname, bool promisc)
  : _ifname(ifname), _promisc(promisc), _pcap(0)
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
  return new FromBPF(_ifname, _promisc);
}

int
FromBPF::configure(const String &conf, ErrorHandler *errh)
{
  if (_pcap) pcap_close(_pcap);
  _pcap = 0;

  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_ifname,
		  cpOptional,
		  cpInteger, "be promiscuous", &_promisc,
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
  
  char *ifname = _ifname.mutable_c_str();
  char ebuf[PCAP_ERRBUF_SIZE];
  _pcap = pcap_open_live(ifname,
                         12000, /* XXX snaplen */
                         _promisc,
                         1,     /* don't batch packets */
                         ebuf);
  if (!_pcap)
    return errh->error("%s: %s", ifname, ebuf);

  return 0;
}

void
FromBPF::get_packet(u_char* clientdata,
		    const struct pcap_pkthdr* pkthdr,
		    const u_char* data) {
  FromBPF *lr = (FromBPF *) clientdata;
  int length = pkthdr->caplen;
  Packet* p = Packet::make(data, length);
  lr->output(0).push(p);
}

void
FromBPF::selected(int)
{
  /*
   * Read and push() one buffer of packets.
   */
  pcap_dispatch(_pcap, -1, FromBPF::get_packet, (u_char *) this);
}

EXPORT_ELEMENT(FromBPF)
