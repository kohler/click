#include <click/config.h>
#include "xokwriter.hh"
#include <click/error.hh>
#include <click/confparse.hh>

#include <stdio.h>
#include <unistd.h>


extern "C" {
#include <vos/net/fast_eth.h>
#include <vos/net/ae_ether.h>
extern int iptable_find_if_name(const char *);
}

#define dprintf if (0) printf


xokWriter::xokWriter(int c)
  : cardno(c)
{
}


xokWriter::xokWriter(const String &ifname)
{
  const char *name = ifname.data();
  cardno = iptable_find_if_name(name);
  if (cardno < 0)
    fprintf(stderr,"interface %s not found\n",name);
}


int
xokWriter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String _ifname;
  int r = cp_va_kparse(conf, this, errh,
		       "DEVNAME", cpkP+cpkM, cpString, &_ifname, cpEnd);
  if (r < 0) return r;

  const char *name = _ifname.data();

  dprintf("xokWriter: looking for interface .%s.\n", name);
  cardno = iptable_find_if_name(name);
  dprintf("it is at %d\n",cardno);
  if (cardno < 0)
  {
    errh->error("interface not found");
    return -1;
  }

  return 0;
}


void
xokWriter::push(int port, Packet *p)
{
  assert(p->length() >= 14);
  assert(cardno >= 0);

  int r = ae_eth_send(p->data(), p->length(), cardno);
  if (r < 0)
    fprintf(stderr, "xokWriter: write to card %d failed, packet dropped\n", 
	    cardno);
  
  p->kill();
}


bool
xokWriter::run_task(Task *)
{
  while (Packet *p = input(0).pull())
    push(0, p);
  return true;
}


EXPORT_ELEMENT(xokWriter)
