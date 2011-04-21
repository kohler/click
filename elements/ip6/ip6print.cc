/*
 * ip6print.{cc,hh} -- dumps simple ip6 information to screen
 * Benjie Chen
 */

#include <click/config.h>
#include "ip6print.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/ip6address.hh>
#if CLICK_USERLEVEL
# include <stdio.h>
#endif
CLICK_DECLS

IP6Print::IP6Print()
{
}

IP6Print::~IP6Print()
{
}

int
IP6Print::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _bytes = 1500;
    _label = "";
    _contents = false;

    if (Args(conf, this, errh)
	.read_p("LABEL", _label)
	.read("CONTENTS", _contents)
	.read("NBYTES", _bytes)
	.complete() < 0)
	return -1;
    return 0;
}

Packet *
IP6Print::simple_action(Packet *p)
{
    String s = "";
    const click_ip6 *iph = (click_ip6*) p->ip_header();

    StringAccum sa;
    if (_label)
	sa << _label << ": ";
    sa << reinterpret_cast<const IP6Address &>(iph->ip6_src)
       << " -> "
       << reinterpret_cast<const IP6Address &>(iph->ip6_dst)
       << " plen " << ntohs(iph->ip6_plen)
       << ", next " << (int)iph->ip6_nxt
       << ", hlim " << (int)iph->ip6_hlim << "\n";
    const unsigned char *data = p->data();
    if (_contents) {
	int amt = 3*_bytes + (_bytes/4+1) + 3*(_bytes/24+1) + 1;

	sa << "  ";
	char *buf = sa.reserve(amt);
	char *orig_buf = buf;

	if (buf) {
	    for (unsigned i = 0; i < _bytes && i < p->length(); i++) {
		sprintf(buf, "%02x", data[i] & 0xff);
		buf += 2;
		if ((i % 24) == 23) {
		    *buf++ = '\n'; *buf++ = ' '; *buf++ = ' ';
		} else if ((i % 4) == 3)
		    *buf++ = ' ';
	    }
	}
	if (orig_buf) {
	    assert(buf <= orig_buf + amt);
	    sa.adjust_length(buf - orig_buf);
	}
    }
    click_chatter("%s", sa.c_str());
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IP6Print)
