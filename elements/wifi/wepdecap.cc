/*
 * wepdecap.{cc,hh} -- decapsultates 802.11 packets
 * John Bicket
 *
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include "wepdecap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/crc32.h>
CLICK_DECLS

WepDecap::WepDecap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

WepDecap::~WepDecap()
{
  MOD_DEC_USE_COUNT;
}

int
WepDecap::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _strict = false;
  if (cp_va_parse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpBool, "Debug", &_debug,
		  "STRICT", cpBool, "strict header check", &_strict,
		  cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
WepDecap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  struct click_wifi *w = (struct click_wifi *) p->data();

  if (!(w->i_fc[1] & WIFI_FC1_WEP)) {
    /* not a wep packet */
    return p;
  }

  /* decrypt the packet first */

  u_int8_t rc4key[WIFI_WEP_IVLEN + WIFI_KEYBUF_SIZE];
  u_int8_t crcbuf[WIFI_WEP_CRCLEN];
  u_int8_t *icv;
  u_int32_t crc;
  memcpy(rc4key, p->data() + sizeof(click_wifi), WIFI_WEP_IVLEN);
  memcpy(rc4key + WIFI_WEP_IVLEN, key, keylen);

  rc4_init(&_rc4, key, WIFI_WEP_IVLEN + keylen);

  /* decrypt data */
  rc4_crypt_skip(&_rc4,
		 p->data() + sizeof(click_wifi) + WIFI_WEP_HEADERSIZE,
		 p->data() + sizeof(click_wifi) + WIFI_WEP_HEADERSIZE,
		 p->length() - (sizeof(click_wifi) + WIFI_WEP_HEADERSIZE + WIFI_WEP_CRCLEN),
		 0);
  /* calculate CRC over unencrypted data */
  crc = update_crc(~0,
		   (char *) (p->data() + sizeof(click_wifi) + WIFI_WEP_HEADERSIZE),
		   p->length() - (sizeof(click_wifi) + WIFI_WEP_HEADERSIZE + WIFI_WEP_CRCLEN));
  /* decrypt ICV and compare to CRC */
  icv = p->data() + (p->length() - WIFI_WEP_CRCLEN);
  rc4_crypt_skip(&_rc4, crcbuf, icv, WIFI_WEP_CRCLEN, 0);
  
  if (crc != ~le32_to_cpu(*(u_int32_t *)crcbuf)) {
    /* packet failed decrypt */
    checked_output_push(1, p);
  }
  /* strip the wep header off */
  memmove((void *)(p->data() + WIFI_WEP_HEADERSIZE), p->data(), sizeof(click_wifi));
  p->pull(WIFI_WEP_HEADERSIZE);
  /* strip the wep crc off the tail of the packet */
  p->take(WIFI_WEP_CRCLEN);
  return p;
}


enum {H_DEBUG};

static String 
WepDecap_read_param(Element *e, void *thunk)
{
  WepDecap *td = (WepDecap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
WepDecap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WepDecap *f = (WepDecap *)e;
  String s = cp_uncomment(in_s);
  switch((int)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}
 
void
WepDecap::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", WepDecap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", WepDecap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
ELEMENT_REQUIRES(rc4)
EXPORT_ELEMENT(WepDecap)
