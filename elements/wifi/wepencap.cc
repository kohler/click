/*
 * wepencap.{cc,hh} -- decapsultates 802.11 packets
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
/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <click/config.h>
#include "wepencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
#include <click/crc32.h>
CLICK_DECLS

WepEncap::WepEncap()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

WepEncap::~WepEncap()
{
  MOD_DEC_USE_COUNT;
}

int
WepEncap::configure(Vector<String> &conf, ErrorHandler *errh)
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
WepEncap::simple_action(Packet *p_in)
{
  WritablePacket *p = p_in->uniqueify();
  struct click_wifi *w = (struct click_wifi *) p->data();

  w->i_fc[1] |= WIFI_FC1_WEP;

  p->push(WIFI_WEP_HEADERSIZE);
  memmove((void *) p->data(), p->data() + WIFI_WEP_HEADERSIZE, sizeof(click_wifi));

  u_int8_t rc4key[WIFI_WEP_IVLEN + WIFI_KEYBUF_SIZE];
  u_int8_t crcbuf[WIFI_WEP_CRCLEN];
  u_int8_t *icv;
  u_int32_t crc;

  
  memcpy(rc4key, p->data() + sizeof(click_wifi), WIFI_WEP_IVLEN);
  memcpy(rc4key + WIFI_WEP_IVLEN, key, keylen);
  rc4_init(&_rc4, rc4key, WIFI_WEP_IVLEN + keylen);
  
  /* calculate CRC over unencrypted data */
  crc = update_crc(~0,
		   (char *)(p->data() + sizeof(click_wifi) + WIFI_WEP_HEADERSIZE),
		   p->length() - (sizeof(click_wifi) + WIFI_WEP_HEADERSIZE)); 

  /* encrypt data */
  rc4_crypt_skip(&_rc4,
		 p->data() + sizeof(click_wifi) + WIFI_WEP_HEADERSIZE,
		 p->data() + sizeof(click_wifi) + WIFI_WEP_HEADERSIZE,
		 p->length() - (sizeof(click_wifi) + WIFI_WEP_HEADERSIZE),
		 0);
  /* tack on ICV */
  *(u_int32_t *)crcbuf = cpu_to_le32(~crc);
  p = p->put(WIFI_WEP_CRCLEN);
  rc4_crypt_skip(&_rc4, icv, crcbuf, WIFI_WEP_CRCLEN, 0);
  return p;
	  
}


enum {H_DEBUG};

static String 
WepEncap_read_param(Element *e, void *thunk)
{
  WepEncap *td = (WepEncap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
WepEncap_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  WepEncap *f = (WepEncap *)e;
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
WepEncap::add_handlers()
{
  add_default_handlers(true);

  add_read_handler("debug", WepEncap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", WepEncap_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
ELEMENT_REQUIRES(rc4)
EXPORT_ELEMENT(WepEncap)
