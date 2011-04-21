#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "packetlogger.hh"
CLICK_DECLS

PacketLogger::PacketLogger()
{
}

PacketLogger::~PacketLogger()
{
}

int
PacketLogger::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read("ETHERTYPE", _et)
	.read("NBYTES", _nb)
	.complete();
}

Packet *
PacketLogger::simple_action(Packet *p_in)
{
  click_ether *e = (click_ether *)p_in->data();
  if (p_in->length() >= sizeof(click_ether) + NBYTES &&
      ntohs(e->ether_type) == _et) {

    log_entry d;
    d.timestamp = p_in->timestamp_anno();
    memcpy(d.src_mac, e->ether_shost, 6);

//    click_chatter("%x %x %x %x %x %x",
//		  e->ether_shost[0],
//		  e->ether_shost[1],
//		  e->ether_shost[2],
//		  e->ether_shost[3],
//		  e->ether_shost[4],
//		  e->ether_shost[5]);
//
//    click_chatter("%x %x %x %x %x %x",
//		  d.src_mac[0],
//		  d.src_mac[1],
//		  d.src_mac[2],
//		  d.src_mac[3],
//		  d.src_mac[4],
//		  d.src_mac[5]);

    memcpy(d.bytes, p_in->data() + sizeof(click_ether), NBYTES);

    _p.push_back(d);
  }
  return p_in;
}

void
PacketLogger::add_handlers()
{
  add_read_handler("log", print_log, 0);
}

#define MAX_PROC_SIZE  16384
String
PacketLogger::print_log(Element *e, void *)
{
  PacketLogger *p = (PacketLogger *)e;

  int bytes_per_entry = 18 + 2 + 17 + 3 + NBYTES * 2 + NBYTES / 4 + 1;
  int n = p->_p.size() * bytes_per_entry;

  n = n > MAX_PROC_SIZE ? MAX_PROC_SIZE : n;
  StringAccum sa(n);
  // click_chatter("have %d packets; n is %d", p->_p.size(), n);

  while (p->_p.size() &&
	 sa.length() < MAX_PROC_SIZE - bytes_per_entry) {
    log_entry d;
    d = p->_p[0];
    p->_p.pop_front();

    sa << d.timestamp << "  ";
    char *buf = sa.data() + sa.length();

    int pos = 0;
    for (unsigned i = 0; i < 6; i++) {
      sprintf(buf + pos, "%02x", d.src_mac[i]);
      pos += 2;
      if (i != 5) buf[pos++] = ':';
    }
    buf[pos++] = ' ';
    buf[pos++] = '|';
    buf[pos++] = ' ';

    for (unsigned i = 0; i < NBYTES; i++) {
      sprintf(buf + pos, "%02x", d.bytes[i]);
      pos += 2;
      if ((i % 4) == 3) { buf[pos++] = ' '; }
    }
    buf[pos++] = '\n';

    sa.adjust_length(pos);
  }

  return sa.take_string();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketLogger)

