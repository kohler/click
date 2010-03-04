#include <click/config.h>
#include "icmpipencap.hh"
#include <click/confparse.hh>
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
    int code;
    if (cp_va_kparse(conf, this, errh,
		     "SRC", cpkP+cpkM, cpIPAddress, &_src,
		     "DST", cpkP+cpkM, cpIPAddress, &_dst,
		     "TYPE", cpkP+cpkM, cpNamedInteger, NameInfo::T_ICMP_TYPE, &_icmp_type,
		     "CODE", cpkP, cpWord, &code_str,
		     "IDENTIFIER", 0, cpUnsignedShort, &_icmp_id,
		     cpEnd) < 0)
	return -1;

    if (!NameInfo::query_int(NameInfo::T_ICMP_CODE + _icmp_type, this, code_str, &code) || code < 0 || code > 255)
	return errh->error("invalid code");
    _icmp_code = code;

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
	WritablePacket *q;
	bool need_seq_number = false;
	switch(_icmp_type) {
		case ICMP_UNREACH:
			if (_icmp_code == ICMP_UNREACH_NEEDFRAG)
				q = p->push(sizeof(click_ip) + sizeof(struct click_icmp_needfrag));
			else
				q = p->push(sizeof(click_ip) + sizeof(struct click_icmp));
			break;
		case ICMP_TSTAMP:
		case ICMP_TSTAMPREPLY:
			q = p->push(sizeof(click_ip) + sizeof(struct click_icmp_tstamp));
			need_seq_number = true;
			break;
		case ICMP_REDIRECT:
			q = p->push(sizeof(click_ip) + sizeof(struct click_icmp_redirect));
			break;
		case ICMP_PARAMPROB:
			q = p->push(sizeof(click_ip) + sizeof(struct click_icmp_paramprob));
			break;
		case ICMP_ECHO:
		case ICMP_ECHOREPLY:
		case ICMP_IREQ:
		case ICMP_IREQREPLY:
			q = p->push(sizeof(click_ip) + sizeof(struct click_icmp_sequenced));
			need_seq_number = true;
			break;
		default:
			q = p->push(sizeof(click_ip) + sizeof(struct click_icmp));
	}

    if (q) {
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

	click_icmp_echo *icmp = (struct click_icmp_echo *) (ip + 1);
	icmp->icmp_type = _icmp_type;
	icmp->icmp_code = _icmp_code;
	icmp->icmp_cksum = 0;
#ifdef __linux__
	icmp->icmp_identifier = _icmp_id;
	if(need_seq_number)
		icmp->icmp_sequence = _ip_id;
#else
	icmp->icmp_identifier = htons(_icmp_id);
	if(need_seq_number)
		icmp->icmp_sequence = htons(_ip_id);
#endif

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
    if (!cp_ip_address(str, &a))
	return errh->error("expected IP address");
    if (thunk)
	i->_dst = a;
    else
	i->_src = a;
    return 0;
}

void ICMPIPEncap::add_handlers()
{
    add_read_handler("src", read_handler, (void *) 0, Handler::CALM);
    add_write_handler("src", write_handler, (void *) 0);
    add_read_handler("dst", read_handler, (void *) 1, Handler::CALM);
    add_write_handler("dst", write_handler, (void *) 1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ICMPIPEncap)
