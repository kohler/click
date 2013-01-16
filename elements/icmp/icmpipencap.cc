#include <click/config.h>
#include "icmpipencap.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/icmp.h>
#include <click/packet_anno.hh>
#include <click/nameinfo.hh>
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

ICMPIPEncap::ICMPIPEncap()
    : _icmp_id(0), _ip_id(1), _icmp_type(0), _icmp_code(0)
{
}

ICMPIPEncap::~ICMPIPEncap()
{
}

int
ICMPIPEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String code_str = "0";
    int icmp_type, icmp_code;
    if (Args(conf, this, errh)
	.read_mp("SRC", _src)
	.read_mp("DST", _dst)
	.read_mp("TYPE", NamedIntArg(NameInfo::T_ICMP_TYPE), icmp_type)
	.read_p("CODE", WordArg(), code_str)
	.read("IDENTIFIER", _icmp_id)
	.complete() < 0)
	return -1;

    if (icmp_type < 0 || icmp_type > 255)
	return errh->error("invalid TYPE");
    if (!NameInfo::query_int(NameInfo::T_ICMP_CODE + icmp_type, this, code_str, &icmp_code)
	|| icmp_code < 0 || icmp_code > 255)
	return errh->error("invalid CODE");
    _icmp_type = icmp_type;
    _icmp_code = icmp_code;

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    { // check alignment
	int ans, c, o;
	ans = AlignmentInfo::query(this, 0, c, o);
	_aligned = (ans && c == 4 && o == 0);
	if (!_aligned)
	    errh->warning("IP header unaligned, cannot use fast IP checksum");
	if (!ans)
	    errh->message("(Try passing the configuration through `click-align'.)");
    }
#endif

    return 0;
}

Packet *
ICMPIPEncap::simple_action(Packet *p)
{
    if (WritablePacket *q = p->push(sizeof(click_ip) + click_icmp_hl(_icmp_type))) {
	click_ip *ip = reinterpret_cast<click_ip *>(q->data());
	ip->ip_v = 4;
	ip->ip_hl = sizeof(click_ip) >> 2;
	ip->ip_tos = 0;
	ip->ip_len = htons(q->length());
	ip->ip_id = htons(_ip_id);
	ip->ip_off = 0;
	ip->ip_ttl = 255;
	ip->ip_p = IP_PROTO_ICMP; /* icmp */
	ip->ip_sum = 0;
	ip->ip_src = _src;
	ip->ip_dst = _dst;

	click_icmp_sequenced *icmp = (struct click_icmp_sequenced *) (ip + 1);
	memset(icmp, 0, click_icmp_hl(_icmp_type));
	icmp->icmp_type = _icmp_type;
	icmp->icmp_code = _icmp_code;

	if(_icmp_type == ICMP_TSTAMP || _icmp_type == ICMP_TSTAMPREPLY || _icmp_type == ICMP_ECHO
		|| _icmp_type == ICMP_ECHOREPLY || _icmp_type == ICMP_IREQ || _icmp_type == ICMP_IREQREPLY) {
		icmp->icmp_identifier = htons(_icmp_id);
		icmp->icmp_sequence = htons(_ip_id);
	}

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
	if (_aligned)
	    ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
	else
	    ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#elif HAVE_FAST_CHECKSUM
	ip->ip_sum = ip_fast_csum((unsigned char *)ip, sizeof(click_ip) >> 2);
#else
	ip->ip_sum = click_in_cksum((unsigned char *)ip, sizeof(click_ip));
#endif
	icmp->icmp_cksum = click_in_cksum((const unsigned char *)icmp, q->length() - sizeof(click_ip));

	q->set_dst_ip_anno(IPAddress(_dst));
	q->set_ip_header(ip, sizeof(click_ip));

	_ip_id += (_ip_id == 0xFFFF ? 2 : 1);
	return q;
    } else
	return 0;
}

String ICMPIPEncap::read_handler(Element *e, void *thunk)
{
    ICMPIPEncap *i = static_cast<ICMPIPEncap *>(e);
    if (thunk)
	return IPAddress(i->_dst).unparse();
    else
	return IPAddress(i->_src).unparse();
}

int ICMPIPEncap::write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    ICMPIPEncap *i = static_cast<ICMPIPEncap *>(e);
    IPAddress a;
    if (!IPAddressArg().parse(str, a))
	return errh->error("syntax error");
    if (thunk)
	i->_dst = a;
    else
	i->_src = a;
    return 0;
}

void ICMPIPEncap::add_handlers()
{
    add_read_handler("src", read_handler, 0, Handler::CALM);
    add_write_handler("src", write_handler, 0);
    add_read_handler("dst", read_handler, 1, Handler::CALM);
    add_write_handler("dst", write_handler, 1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPIPEncap)
