// -*- c-basic-offset: 4 -*-

#include <config.h>
#include <click/config.h>

#include "aggregateip.hh"
#include <click/error.hh>
#include <click/hashmap.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
#include <click/packet_anno.hh>

static HashMap<String, int> *chunkmap;
static Vector<AggregateIP::Field> chunks;
static Vector<int> chunk_next;

#define AG_NONE			0
#define AG_IP			1
#define AG_TRANSP		2
#define AG_PAYLOAD		3

#define AG_TRANSP_P(what)	(AG_TRANSP | ((what) << 8))
#define AG_PROTO(t)		((t) >> 8)

static void
add_chunk(const String &name, const AggregateIP::Field &f)
{
    int n = (*chunkmap)[name];
    int i = chunks.size();
    chunks.push_back(f);
    chunk_next.push_back(n);
    chunkmap->insert(name, i);
}

void
AggregateIP::Field::initialize_wordmap()
{
    if (chunkmap) delete chunkmap;
    chunkmap = new HashMap<String, int>(-1);

    add_chunk("vers",	Field(AG_IP, 0*8, 4));
    add_chunk("hl",	Field(AG_IP, 0*8+4, 4));
    add_chunk("tos",	Field(AG_IP, 1*8, 8));
    add_chunk("dscp",	Field(AG_IP, 1*8, 6));
    add_chunk("ecn",	Field(AG_IP, 1*8+6, 2));
    add_chunk("len",	Field(AG_IP, 2*8, 16));
    add_chunk("id",	Field(AG_IP, 4*8, 16));
    add_chunk("off",	Field(AG_IP, 6*8, 16));
    add_chunk("rf",	Field(AG_IP, 6*8, 1));
    add_chunk("df",	Field(AG_IP, 6*8+1, 1));
    add_chunk("mf",	Field(AG_IP, 6*8+2, 1));
    add_chunk("fragoff",Field(AG_IP, 6*8+3, 13));
    add_chunk("ttl",	Field(AG_IP, 8*8, 8));
    add_chunk("proto",	Field(AG_IP, 9*8, 8));
    add_chunk("csum",	Field(AG_IP, 10*8, 16));
    add_chunk("sum",	Field(AG_IP, 10*8, 16));
    add_chunk("src",	Field(AG_IP, 12*8, 32));
    add_chunk("dst",	Field(AG_IP, 16*8, 32));
    
    add_chunk("sport",	Field(AG_TRANSP, IP_PROTO_TCP_OR_UDP, 0*8, 16));
    add_chunk("dport",	Field(AG_TRANSP, IP_PROTO_TCP_OR_UDP, 2*8, 16));

    add_chunk("len",	Field(AG_TRANSP, IP_PROTO_UDP,	4*8, 16));
    add_chunk("csum",	Field(AG_TRANSP, IP_PROTO_UDP,	6*8, 16));
    add_chunk("sum",	Field(AG_TRANSP, IP_PROTO_UDP,	6*8, 16));

    add_chunk("seq",	Field(AG_TRANSP, IP_PROTO_TCP,	4*8, 32));
    add_chunk("seqno",	Field(AG_TRANSP, IP_PROTO_TCP,	4*8, 32));
    add_chunk("ackno",	Field(AG_TRANSP, IP_PROTO_TCP,	8*8, 32));
    add_chunk("hl",	Field(AG_TRANSP, IP_PROTO_TCP,	12*8, 4));
    add_chunk("flags",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8, 8));
    add_chunk("fin",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8+7, 1));
    add_chunk("syn",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8+6, 1));
    add_chunk("rst",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8+5, 1));
    add_chunk("psh",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8+4, 1));
    add_chunk("push",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8+4, 1));
    add_chunk("ack",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8+3, 1));
    add_chunk("urg",	Field(AG_TRANSP, IP_PROTO_TCP,	13*8+2, 1));
    add_chunk("win",	Field(AG_TRANSP, IP_PROTO_TCP,	14*8, 16));
    add_chunk("window",	Field(AG_TRANSP, IP_PROTO_TCP,	14*8, 16));
    add_chunk("csum",	Field(AG_TRANSP, IP_PROTO_TCP,	16*8, 16));
    add_chunk("sum",	Field(AG_TRANSP, IP_PROTO_TCP,	16*8, 16));
    add_chunk("urp",	Field(AG_TRANSP, IP_PROTO_TCP,	18*8, 16));

    add_chunk("type",	Field(AG_TRANSP, IP_PROTO_ICMP,	0*8, 8));
    add_chunk("code",	Field(AG_TRANSP, IP_PROTO_ICMP,	1*8, 8));
    add_chunk("csum",	Field(AG_TRANSP, IP_PROTO_ICMP,	2*8, 16));
    add_chunk("sum",	Field(AG_TRANSP, IP_PROTO_ICMP,	2*8, 16));
}

static bool
protos_compatible(int p, int q)
{
    if (p == q || !q
	|| (p == IP_PROTO_TCP_OR_UDP && (q == IP_PROTO_TCP || q == IP_PROTO_UDP))
	|| (q == IP_PROTO_TCP_OR_UDP && (p == IP_PROTO_TCP || p == IP_PROTO_UDP)))
	return true;
    else
	return false;
}

const char *
AggregateIP::Field::unparse_type() const
{
    switch (_type) {

      case AG_NONE:	return "<none>";
      case AG_IP:	return "ip";
      case AG_PAYLOAD:	return "data";

      case AG_TRANSP:
	switch (_proto) {
	  case IP_PROTO_TCP:		return "tcp";
	  case IP_PROTO_UDP:		return "udp";
	  case IP_PROTO_ICMP:		return "icmp";
	  case IP_PROTO_TCP_OR_UDP:	return "tcpudp";
	  case 0:			return "transp";
	  default:			return "<error>";
	}

      default:		return "<error>";
	
    }
}

int
AggregateIP::Field::add_word(String name, ErrorHandler *errh)
{
    int i = (*chunkmap)[name];
    int found = -1;
    int found_any = 0;

    for (; i >= 0; i = chunk_next[i], found_any++) {
	const Field &f = chunks[i];
	if (_type != AG_NONE && _type != f._type)
	    continue;
	if (_type == AG_TRANSP && !protos_compatible(f._proto, _proto))
	    continue;
	if (found >= 0)
	    found = -2;
	else if (found == -1)
	    found = i;
    }

    // found unique?
    if (found >= 0) {
	_type = chunks[found]._type;
	_proto = chunks[found]._proto;
	_offset = chunks[found]._offset;
	_length = chunks[found]._length;
	return 0;
    }

    // easy errors
    if (!name)
	return errh->error("missing data field name");
    else if (!found_any)
	return errh->error("unknown data field `%s'", name.cc()); 

    // hard errors: develop list of valid words
    StringAccum sa;
    for (i = (*chunkmap)[name]; i >= 0; i = chunk_next[i]) {
	const Field &f = chunks[i];
	if (_type != AG_NONE && _type != f._type && found != -1)
	    continue;
	if (_type == AG_TRANSP && found == -2 && !protos_compatible(f._proto, _proto))
	    continue;
	if (sa.length())
	    sa << ", ";
	sa << f.unparse_type();
    }
  
    if (found == -2)
	return errh->error("ambiguous data field `%s'\n(specify one of %s)", name.cc(), sa.cc());
    else
	return errh->error("no such data field `%s %s'\n(`%s' is valid for %s)", unparse_type(), name.cc(), name.cc(), sa.cc());
}

int
AggregateIP::Field::apply_mask(uint32_t mask, const char *name, ErrorHandler *errh)
{
    if (!mask)
	return errh->error("`%s' must contain exactly one contiguous set of 1 bits", name);
    if (_length < 32 && mask >= (1U << _length))
	return errh->error("`%s' value `%u' is longer than base field", name, mask);

    uint32_t tester = 1U << (_length - 1);
    uint32_t length_in = _length;
    while ((mask & tester) == 0)
	_offset++, _length--, mask <<= 1;

    uint32_t nmask = htonl(mask << (32 - length_in));
    int prefix = IPAddress(nmask).mask_to_prefix_len();
    if (prefix < 0)
	return errh->error("`%s' must contain exactly one contiguous set of 1 bits", name);
    
    assert(_length > 0 && (uint32_t)prefix <= _length);
    _length = prefix;
    return 0;
}

int
AggregateIP::Field::parse(const String &instr, ErrorHandler *errh)
{
    // create chunkmap if necessary
    if (!chunkmap)
	initialize_wordmap();

    String str = cp_unquote(instr);
    const char *s = str.cc();	// need to make it \0-terminated
    int len = str.length();
    int pos = 0;
    if (pos >= len || !isalnum(s[pos]))
	return errh->error("missing data field");
    
    // read first word
    while (pos < len && isalnum(s[pos]))
	pos++;
    String wd = str.substring(0, pos);

    // is it a data type?
    if (wd == "ip")
	_type = AG_IP;
    else if (wd == "tcp")
	_type = AG_TRANSP, _proto = IP_PROTO_TCP;
    else if (wd == "udp")
	_type = AG_TRANSP, _proto = IP_PROTO_UDP;
    else if (wd == "tcpudp")
	_type = AG_TRANSP, _proto = IP_PROTO_TCP_OR_UDP;
    else if (wd == "icmp")
	_type = AG_TRANSP, _proto = IP_PROTO_ICMP;
    else if (wd == "transp" || wd == "transport")
	_type = AG_TRANSP, _proto = 0;
    else if (wd == "data")
	_type = AG_PAYLOAD;
    else
	pos = 0;

    // skip more whitespace
    while (pos < len && isspace(s[pos]))
	pos++;

    if (pos >= len) {
	return errh->error("missing data field");
	
    } else if (s[pos] == '[' || s[pos] == '{') {
	unsigned consumed, v1, v2;
	char brack = s[pos];
	if (sscanf(s + pos + 1, "%u , %u %n", &v1, &v2, &consumed) >= 2)
	    /* nada */;
	else if (sscanf(s + pos + 1, "%u - %u %n", &v1, &v2, &consumed) >= 2)
	    v2 = v2 - v1 + 1;
	else if (sscanf(s + pos + 1, "%u + %u %n", &v1, &v2, &consumed) >= 2)
	    /* nada */;
	else if (sscanf(s + pos + 1, "%u %n", &v1, &consumed) >= 1)
	    v2 = 1;
	else
	    return errh->error("syntax error after `%c' (expected `%s[OFF]', `%s[OFF1, OFF2]', or `%s[OFF+LEN]')", brack, wd.cc(), wd.cc(), wd.cc());

	pos += 1 + consumed;
	char close_brack = (brack == '[' ? ']' : '}');
	if (pos >= len || s[pos] != close_brack) 
	    errh->error("missing `%c' %c (expected `%s[OFF]', `%s[OFF1, OFF2]', or `%s[OFF+LEN]')", close_brack, s[pos], wd.cc(), wd.cc(), wd.cc());
	else
	    pos++;
	
	if ((int)v2 <= 0)
	    return errh->error("nonpositive length");
    
	_offset = v1*(brack == '[' ? 8 : 1);
	_length = v2*(brack == '[' ? 8 : 1);
	// fall below to skip whitespace
	
    } else {
	// read another word
	int first = pos;
	while (pos < len && isalnum(s[pos]))
	    pos++;
	
	if (add_word(str.substring(first, pos - first), errh) < 0)
	    return -1;
    }

    if (pos == len)
	return 0;

    // check for a specifier after the data field
    while (pos < len && isspace(s[pos]))
	pos++;
    if (pos == len)
	return errh->error("garbage after data field");

    uint32_t data;
    if (s[pos] == '/') {
	for (pos++; pos < len && isspace(s[pos]); pos++)
	    /* nada */;
	if (cp_unsigned(str.substring(pos), &data)) {
	    if (data == 0 || data > _length)
		return errh->error("`/NUM' should be between `/1' and `/%u'", _length);
	    _length = data;
	} else if (cp_ip_address(str.substring(pos), (unsigned char *)&data)) {
	    if (_length != 32)
		return errh->error("`/IPADDR' only usable with IP addresses");
	    return apply_mask(ntohl(data), "/IPADDR", errh);
	} else
	    return errh->error("garbage after data field: expected `/NUM' or `/IPADDR'");
    } else if (s[pos] == '&') {
	for (pos++; pos < len && isspace(s[pos]); pos++)
	    /* nada */;
	if (cp_unsigned(str.substring(pos), &data))
	    return apply_mask(data, "& MASK", errh);
	else
	    return errh->error("garbage after data field: expected `& MASK'");
    } else
	return errh->error("garbage after data field");

    return 0;
}


void
AggregateIP::Field::unparse(StringAccum &sa) const
{
    sa << unparse_type();

    // look for corresponding chunk
    int found = -1;
    for (int i = 0; i < chunks.size() && found < 0; i++) {
	const Field &f = chunks[i];
	if (_type == f._type && _offset >= f._offset
	    && _offset + _length <= f._offset + f._length
	    && (_type != AG_TRANSP || protos_compatible(_proto, f._proto)))
	    found = i;
    }

    if (found >= 0) {
	HashMap<String, int>::Iterator iter = chunkmap->first();
	for (; iter; iter++)
	    if (iter.value() == found) {
		const Field &f = chunks[found];
		sa << ' ' << iter.key();
		if (_offset == f._offset && _length == f._length)
		    /* nada */;
		else if (_offset == f._offset)
		    sa << '/' << _length;
		else {
		    uint32_t mask = (_length == 32 ? 0xFFFFFFFF : (1 << _length) - 1);
		    uint32_t val = (mask << (f._offset + f._length - _offset - _length));
		    if (char *x = sa.reserve(20)) {
			int n;
			sprintf(x, " & 0x%X%n", val, &n);
			sa.forward(n);
		    }
		}
		return;
	    }
    }

    if ((_offset % 8) == 0 && _length == 8)
	sa << '[' << _offset/8 << ']';
    else if ((_offset % 8) == 0 && (_length % 8) == 0)
	sa << '[' << _offset/8 << ", " << _length/8 << ']';
    else
	sa << '{' << _offset << ", " << _length << '}';
}

String
AggregateIP::Field::unparse() const
{
    StringAccum sa;
    unparse(sa);
    return sa.take_string();
}

// ====================================

AggregateIP::AggregateIP()
    : Element(1, 1)
{
    MOD_INC_USE_COUNT;
}

AggregateIP::~AggregateIP()
{
    MOD_DEC_USE_COUNT;
}

void
AggregateIP::notify_noutputs(int n)
{
    set_noutputs(n <= 1 ? 1 : 2);
}

int
AggregateIP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String arg;
    _incremental = false;
    if (cp_va_parse(conf, this, errh,
		    cpArgument, "field specification", &arg,
		    cpKeywords,
		    "INCREMENTAL", cpBool, "incremental?", &_incremental,
		    0) < 0)
	return -1;
    if (_f.parse(arg, errh) < 0)
	return -1;
    uint32_t right = _f.offset() + _f.length() - 1;
    if ((_f.offset() / 32) != (right / 32))
	return errh->error("field specification does not fit within a single word");
    if (_f.length() > 32)
	return errh->error("too many aggregates: field length too large, max 32");
    else if (_f.length() == 32 && _incremental)
	return errh->error("`INCREMENTAL' is incompatible with field length 32");
    if (_f.length() == 32)
	_mask = 0xFFFFFFFFU;
    else
	_mask = (1 << _f.length()) - 1;
    _offset = (_f.offset() / 32) * 4;
    _shift = 31 - right % 32;
    //errh->message("%d, %d -> %d %d %d", _f.offset(), _f.length(), _offset, _shift, _mask);
    return 0;
}

Packet *
AggregateIP::bad_packet(Packet *p)
{
    if (noutputs() == 2)
	output(1).push(p);
    else
	p->kill();
    return 0;
}

Packet *
AggregateIP::handle_packet(Packet *p)
{
    const click_ip *iph = p->ip_header();
    if (!iph)
	return bad_packet(p);
    
    int offset = p->length();
    switch (_f.type()) {
      case AG_IP:
	offset = p->network_header_offset();
	break;
      case AG_TRANSP:
	if (ntohs(iph->ip_off) & IP_OFFMASK)
	    /* bad; will be thrown away below */;
	else if (iph->ip_p == _f.proto() || protos_compatible(iph->ip_p, _f.proto()))
	    offset = p->transport_header_offset();
	break;
      case AG_PAYLOAD:
	if (ntohs(iph->ip_off) & IP_OFFMASK)
	    /* bad; will be thrown away below */;
	else if (iph->ip_p == IPPROTO_TCP && p->transport_header_offset() + sizeof(click_tcp) <= p->length()) {
	    const click_tcp *tcph = (const click_tcp *)p->transport_header();
	    offset = p->transport_header_offset() + (tcph->th_off << 2);
	} else if (iph->ip_p == IPPROTO_UDP)
	    offset = p->transport_header_offset() + sizeof(click_udp);
	break;
    }
    offset += _offset;

    if (offset + 4 > (int)p->length())
	return bad_packet(p);

    uint32_t udata = *((const uint32_t *)(p->data() + offset));
    uint32_t agg = (ntohl(udata) >> _shift) & _mask;

    if (_incremental)
	SET_AGGREGATE_ANNO(p, AGGREGATE_ANNO(p)*(_mask + 1) + agg);
    else
	SET_AGGREGATE_ANNO(p, agg);

    return p;
}

void
AggregateIP::push(int, Packet *p)
{
    if (Packet *q = handle_packet(p))
	output(0).push(q);
}

Packet *
AggregateIP::pull(int)
{
    Packet *p = input(0).pull();
    if (p)
	p = handle_packet(p);
    return p;
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AggregateIP)

#include <click/vector.cc>
