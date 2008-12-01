/*
 * checkipheader.{cc,hh} -- element checks IP header for correctness
 * (checksums, lengths, source addresses)
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2003 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "checkipheader.hh"
#include <clicknet/ip.h>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/standard/alignmentinfo.hh>
#ifdef CLICK_LINUXMODULE
# include <net/checksum.h>
#endif
CLICK_DECLS

const char * const CheckIPHeader::reason_texts[NREASONS] = {
  "tiny packet", "bad IP version", "bad IP header length",
  "bad IP length", "bad IP checksum", "bad source address"
};

#define IPADDR_LIST_INTERFACES		((void *)0)
#define IPADDR_LIST_BADSRC		((void *)1)
#define IPADDR_LIST_BADSRC_OLD		((void *)2)

static void
ipaddr_list_parse(cp_value *v, const String &arg, ErrorHandler *errh, const char *argname, Element *context)
{
    Vector<String> words;
    cp_spacevec(arg, words);
    if (v->argtype->user_data == IPADDR_LIST_BADSRC_OLD) {
	words.push_back(String::make_stable("0.0.0.0", 7));
	words.push_back(String::make_stable("255.255.255.255", 15));
    }

    bool addresses = (v->argtype->user_data != IPADDR_LIST_INTERFACES);
    String str = String::make_garbage((addresses ? 4 : 8) * words.size());
    if (str.out_of_memory()) {
	errh->error("out of memory!");
	return;
    }
    unsigned char *data = (unsigned char *) str.mutable_data();

    for (int i = 0; i < words.size(); i++)
	if (addresses ? !cp_ip_address(words[i], &data[i*4], context)
	    : !cp_ip_prefix(words[i], &data[i*8], &data[i*8+4], true, context)) {
	    errh->error("%s takes list of IP %s", argname, addresses ? "addresses" : "prefixes");
	    return;
	}

    v->v_string = str;
}

static void
ipaddr_list_store(cp_value *v, Element *)
{
    Vector<IPAddress> *l1 = (Vector<IPAddress> *)v->store;
    l1->clear();
    const uint32_t *vec = reinterpret_cast<const uint32_t *>(v->v_string.data());
    int len = v->v_string.length() / 4;

    if (v->argtype->user_data == IPADDR_LIST_INTERFACES) {
	for (int i = 0; i < len/2; i++)
	    l1->push_back((vec[i*2] & vec[i*2 + 1]) | ~vec[i*2 + 1]);
	l1->push_back(0x00000000U);
	l1->push_back(0xFFFFFFFFU);

	Vector<IPAddress> *l2 = (Vector<IPAddress> *)v->store2;
	l2->clear();
	for (int i = 0; i < len/2; i++)
	    l2->push_back(vec[i*2]);
    } else {
	for (int i = 0; i < len; i++)
	    l1->push_back(vec[i]);
    }
}

void
CheckIPHeader::static_initialize()
{
  cp_register_argtype("CheckIPHeader.INTERFACES", "list of router IP addresses and prefixes", cpArgStore2, ipaddr_list_parse, ipaddr_list_store, IPADDR_LIST_INTERFACES);
  cp_register_argtype("CheckIPHeader.BADSRC", "list of IP addresses", cpArgNormal, ipaddr_list_parse, ipaddr_list_store, IPADDR_LIST_BADSRC);
  cp_register_argtype("CheckIPHeader.BADSRC_OLD", "list of IP addresses", cpArgNormal, ipaddr_list_parse, ipaddr_list_store, IPADDR_LIST_BADSRC_OLD);
}

void
CheckIPHeader::static_cleanup()
{
  cp_unregister_argtype("CheckIPHeader.INTERFACES");
  cp_unregister_argtype("CheckIPHeader.BADSRC");
  cp_unregister_argtype("CheckIPHeader.BADSRC_OLD");
}

CheckIPHeader::CheckIPHeader()
  : _checksum(true), _reason_drops(0)
{
  _drops = 0;
}

CheckIPHeader::~CheckIPHeader()
{
  delete[] _reason_drops;
}

int
CheckIPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _offset = 0;
  bool verbose = false;
  bool details = false;

  if (cp_va_kparse_remove_keywords(conf, this, errh,
		"INTERFACES", 0, "CheckIPHeader.INTERFACES", &_bad_src, &_good_dst,
		"BADSRC", 0, "CheckIPHeader.BADSRC", &_bad_src,
		"GOODDST", 0, "CheckIPHeader.BADSRC", &_good_dst,
		"OFFSET", 0, cpUnsigned, &_offset,
		"VERBOSE", 0, cpBool, &verbose,
		"DETAILS", 0, cpBool, &details,
		"CHECKSUM", 0, cpBool, &_checksum,
		cpEnd) < 0)
    return -1;

  if (conf.size() == 1 && cp_integer(conf[0], &_offset))
    /* nada */;
  else if (cp_va_kparse(conf, this, errh,
			"BADSRC*", cpkP, "CheckIPHeader.BADSRC_OLD", &_bad_src,
			"OFFSET", cpkP, cpUnsigned, &_offset,
			cpEnd) < 0)
    return -1;

  _verbose = verbose;
  if (details) {
      _reason_drops = new atomic_uint32_t[NREASONS];
      for (int i = 0; i < NREASONS; ++i)
	  _reason_drops[i] = 0;
  }

#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  // check alignment
  if (_checksum) {
    int ans, c, o;
    ans = AlignmentInfo::query(this, 0, c, o);
    o = (o + 4 - (_offset % 4)) % 4;
    _aligned = (ans && c == 4 && o == 0);
    if (!_aligned)
      errh->warning("IP header unaligned, cannot use fast IP checksum");
    if (!ans)
      errh->message("(Try passing the configuration through `click-align'.)");
  }
#endif

  //for (int i = 0; i < _bad_src.n; i++)
  //  click_chatter("bad: %s", IPAddress(_bad_src.vec[i]).s().c_str());
  //for (int i = 0; i < _good_dst.n; i++)
  //  click_chatter("good: %s", IPAddress(_good_dst.vec[i]).s().c_str());

  return 0;
}

Packet *
CheckIPHeader::drop(Reason reason, Packet *p)
{
    if (_drops == 0 || _verbose)
	click_chatter("%s: IP header check failed: %s", name().c_str(), reason_texts[reason]);
    _drops++;

    if (_reason_drops)
	_reason_drops[reason]++;

    if (noutputs() == 2)
	output(1).push(p);
    else
	p->kill();

    return 0;
}

Packet *
CheckIPHeader::simple_action(Packet *p)
{
  const click_ip *ip = reinterpret_cast<const click_ip *>(p->data() + _offset);
  unsigned plen = p->length() - _offset;
  unsigned hlen, len;

  // cast to int so very large plen is interpreted as negative
  if ((int)plen < (int)sizeof(click_ip))
    return drop(MINISCULE_PACKET, p);

  if (ip->ip_v != 4)
    return drop(BAD_VERSION, p);

  hlen = ip->ip_hl << 2;
  if (hlen < sizeof(click_ip))
    return drop(BAD_HLEN, p);

  len = ntohs(ip->ip_len);
  if (len > plen || len < hlen)
    return drop(BAD_IP_LEN, p);

  if (_checksum) {
    int val;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
    if (_aligned)
      val = ip_fast_csum((unsigned char *)ip, ip->ip_hl);
    else
      val = click_in_cksum((const unsigned char *)ip, hlen);
#elif HAVE_FAST_CHECKSUM
    val = ip_fast_csum((unsigned char *)ip, ip->ip_hl);
#else
    val = click_in_cksum((const unsigned char *)ip, hlen);
#endif
    if (val != 0)
      return drop(BAD_CHECKSUM, p);
  }

  /*
   * RFC1812 5.3.7 and 4.2.2.11: discard illegal source addresses.
   * Configuration string should have listed all subnet
   * broadcast addresses known to this router.
   */
  if (find(_bad_src.begin(), _bad_src.end(), IPAddress(ip->ip_src)) < _bad_src.end()
      && find(_good_dst.begin(), _good_dst.end(), IPAddress(ip->ip_dst)) == _good_dst.end())
    return drop(BAD_SADDR, p);

  /*
   * RFC1812 4.2.3.1: discard illegal destinations.
   * We now do this in the IP routing table.
   */

  p->set_ip_header(ip, hlen);

  // shorten packet according to IP length field -- 7/28/2000
  if (plen > len)
    p->take(plen - len);

  // set destination IP address annotation if it doesn't exist already --
  // 9/26/2001
  // always set destination IP address annotation; linuxmodule problem
  // reported by Parveen Kumar Patel at Utah -- 4/3/2002
  p->set_dst_ip_anno(ip->ip_dst);

  return(p);
}

String
CheckIPHeader::read_handler(Element *e, void *)
{
  CheckIPHeader *c = reinterpret_cast<CheckIPHeader *>(e);
  StringAccum sa;
  for (int i = 0; i < NREASONS; i++)
      sa << c->_reason_drops[i] << '\t' << reason_texts[i] << '\n';
  return sa.take_string();
}

void
CheckIPHeader::add_handlers()
{
    add_data_handlers("drops", Handler::OP_READ, &_drops);
    if (_reason_drops)
	add_read_handler("drop_details", read_handler, (void *)1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckIPHeader)
