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

static int checkipheader_count = 0;

static void
ipaddr_list_parse(cp_value *v, const String &arg, ErrorHandler *errh, const char *argname, Element *context)
{
  Vector<String> words;
  cp_spacevec(arg, words);

  StringAccum sa;
  IPAddress addr[2];

  if (v->argtype->user_data == IPADDR_LIST_INTERFACES) {
    for (int i = 0; i < words.size(); i++)
      if (cp_ip_prefix(words[i], &addr[0], &addr[1], true, context))
	memcpy(sa.extend(8), &addr[0], 8);
      else {
	errh->error("%s takes list of IP prefixes (%s)", argname, v->description);
	return;
      }
  } else {
    for (int i = 0; i < words.size(); i++)
      if (cp_ip_address(words[i], &addr[0], context))
	memcpy(sa.extend(4), &addr[0], 4);
      else {
	errh->error("%s takes list of IP addresses (%s) [%s]", argname, v->description, words[i].cc());
	return;
      }
    if (v->argtype->user_data == IPADDR_LIST_BADSRC_OLD)
      memcpy(sa.extend(8), "\x00\x00\x00\x00\xFF\xFF\xFF\xFF", 8);
  }

  v->v_string = sa.take_string();
}

static void
ipaddr_list_store(cp_value *v, Element *)
{
  IPAddressList *l1 = (IPAddressList *)v->store;
  const uint32_t *vec = reinterpret_cast<const uint32_t *>(v->v_string.data());
  int len = v->v_string.length() / 4;

  if (v->argtype->user_data == IPADDR_LIST_INTERFACES) {
    uint32_t *l = new uint32_t[len/2 + 2];
    for (int i = 0; i < len/2; i++)
      l[i] = (vec[i*2] & vec[i*2 + 1]) | ~vec[i*2 + 1];
    l[len/2] = 0x00000000U;
    l[len/2+1] = 0xFFFFFFFFU;
    l1->assign(len/2 + 2, l);

    IPAddressList *l2 = (IPAddressList *)v->store2;
    l = new uint32_t[len/2];
    for (int i = 0; i < len/2; i++)
      l[i] = vec[i*2];
    l2->assign(len/2, l);
  } else {
    uint32_t *l = new uint32_t[len];
    memcpy(l, vec, len * sizeof(uint32_t));
    l1->assign(len, l);
  }
}

static void
checkipheader_static_initialize()
{
  if (++checkipheader_count == 1) {
    cp_register_argtype("CheckIPHeader.INTERFACES", "list of router IP addresses and prefixes", cpArgStore2, ipaddr_list_parse, ipaddr_list_store, IPADDR_LIST_INTERFACES);
    cp_register_argtype("CheckIPHeader.BADSRC", "list of IP addresses", cpArgNormal, ipaddr_list_parse, ipaddr_list_store, IPADDR_LIST_BADSRC);
    cp_register_argtype("CheckIPHeader.BADSRC_OLD", "list of IP addresses", cpArgNormal, ipaddr_list_parse, ipaddr_list_store, IPADDR_LIST_BADSRC_OLD);
  }
}

static void
checkipheader_static_cleanup()
{
  if (--checkipheader_count == 0) {
    cp_unregister_argtype("CheckIPHeader.INTERFACES");
    cp_unregister_argtype("CheckIPHeader.BADSRC");
    cp_unregister_argtype("CheckIPHeader.BADSRC_OLD");
  }
}

CheckIPHeader::CheckIPHeader()
  : _checksum(true), _reason_drops(0)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
  _drops = 0;
  checkipheader_static_initialize();
}

CheckIPHeader::~CheckIPHeader()
{
  MOD_DEC_USE_COUNT;
  delete[] _reason_drops;
  checkipheader_static_cleanup();
}

CheckIPHeader *
CheckIPHeader::clone() const
{
  return new CheckIPHeader();
}

void
CheckIPHeader::notify_noutputs(int n)
{
  set_noutputs(n < 2 ? 1 : 2);
}

int
CheckIPHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _offset = 0;
  bool verbose = false;
  bool details = false;

  if (cp_va_parse_remove_keywords(conf, 0, this, errh,
		"INTERFACES", "CheckIPHeader.INTERFACES", "router interface addresses", &_bad_src, &_good_dst,
		"BADSRC", "CheckIPHeader.BADSRC", "bad source addresses", &_bad_src,
		"GOODDST", "CheckIPHeader.BADSRC", "good destination addresses", &_good_dst,
		"OFFSET", cpUnsigned, "IP header offset", &_offset,
		"VERBOSE", cpBool, "be verbose?", &verbose,
		"DETAILS", cpBool, "keep detailed counts?", &details,
		"CHECKSUM", cpBool, "check checksum?", &_checksum,
		0) < 0)
    return -1;

  if (conf.size() == 1 && cp_unsigned(conf[0], &_offset))
    /* nada */;
  else if (cp_va_parse(conf, this, errh, cpOptional,
		       "CheckIPHeader.BADSRC_OLD", "bad source addresses", &_bad_src,
		       cpUnsigned, "IP header offset", &_offset,
		       0) < 0)
    return -1;

  _verbose = verbose;
  if (details)
    _reason_drops = new uatomic32_t[NREASONS];

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
  //  click_chatter("bad: %s", IPAddress(_bad_src.vec[i]).s().cc());
  //for (int i = 0; i < _good_dst.n; i++)
  //  click_chatter("good: %s", IPAddress(_good_dst.vec[i]).s().cc());
  
  return 0;
}

Packet *
CheckIPHeader::drop(Reason reason, Packet *p)
{
  if (_drops == 0 || _verbose)
    click_chatter("IP header check failed: %s", reason_texts[reason]);
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
  if (_bad_src.contains(ip->ip_src)
      && !_good_dst.contains(ip->ip_dst))
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
CheckIPHeader::read_handler(Element *e, void *thunk)
{
  CheckIPHeader *c = reinterpret_cast<CheckIPHeader *>(e);
  switch ((intptr_t)thunk) {

   case 0:			// drops
    return String(c->_drops) + "\n";

   case 1: {			// drop_details
     StringAccum sa;
     for (int i = 0; i < NREASONS; i++)
       sa << c->_reason_drops[i] << '\t' << reason_texts[i] << '\n';
     return sa.take_string();
   }

   default:
    return String("<error>\n");

  }
}

void
CheckIPHeader::add_handlers()
{
  add_read_handler("drops", read_handler, (void *)0);
  if (_reason_drops)
    add_read_handler("drop_details", read_handler, (void *)1);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckIPHeader)
