/*
 * sha1.{cc,hh} -- element implements IPsec SHA1 authentication (RFC 2404)
 * Benjie Chen
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#ifndef HAVE_IPSEC
# error "Must #define HAVE_IPSEC in config.h"
#endif
#include "sha1.hh"
#include "esp.hh"
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <click/error.hh>
#include <click/glue.hh>
#include "elements/ipsec/sha1_impl.hh"
CLICK_DECLS

#define SHA_DIGEST_LEN 20

IPsecAuthSHA1::IPsecAuthSHA1()
{
}

IPsecAuthSHA1::~IPsecAuthSHA1()
{
}

int
IPsecAuthSHA1::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("VERIFY", _op).complete();
}

int
IPsecAuthSHA1::initialize(ErrorHandler *)
{
  _drops = 0;
  return 0;
}


Packet *
IPsecAuthSHA1::simple_action(Packet *p)
{
  // compute sha1
  if (_op == COMPUTE_AUTH) {
    unsigned char digest [SHA_DIGEST_LEN];
    SHA1_ctx ctx;
    SHA1_init (&ctx);
    SHA1_update (&ctx, (u_char*) p->data(), p->length());
    SHA1_final (digest, &ctx);
    WritablePacket *q = p->put(12);
    u_char *ah = ((u_char*)q->data())+q->length()-12;
    memmove(ah, digest, 12);
    return q;
  }

  else {
    const u_char *ah = p->data()+p->length()-12;
    unsigned char digest [SHA_DIGEST_LEN];
    SHA1_ctx ctx;
    SHA1_init (&ctx);
    SHA1_update (&ctx, (u_char*) p->data(), p->length()-12);
    SHA1_final (digest, &ctx);
    if (memcmp(ah, digest, 12)) {
      if (_drops == 0)
	click_chatter("Invalid SHA1 authentication digest");
      _drops++;
      if (noutputs() > 1)
	output(1).push(p);
      else
	p->kill();
      return 0;
    }
    p->take(12);
    return p;
  }
}

String
IPsecAuthSHA1::drop_handler(Element *e, void *)
{
  IPsecAuthSHA1 *a = (IPsecAuthSHA1 *)e;
  return String(a->_drops);
}

void
IPsecAuthSHA1::add_handlers()
{
  add_read_handler("drops", drop_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPsecAuthSHA1)
ELEMENT_MT_SAFE(IPsecAuthSHA1)
