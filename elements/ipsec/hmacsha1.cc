/*
 * hmacsha1.{cc,hh} -- element implements IPsec hmac authentication using SHA1
 * Dimitris Syrivelis
 *
 * Copyright (c) 2006 University of Thessaly, Hellas
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
#include "hmacsha1.hh"
#include "esp.hh"
#include <click/ipaddress.hh>
#include <click/args.hh>
#include <clicknet/ip.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>

#include "elements/ipsec/hmac.hh"
#include "satable.hh"
#include "sadatatuple.hh"
CLICK_DECLS

#define SHA_DIGEST_LEN 20
#define KEY_SIZE 16

IPsecAuthHMACSHA1::IPsecAuthHMACSHA1()
{
}

IPsecAuthHMACSHA1::~IPsecAuthHMACSHA1()
{
}

int
IPsecAuthHMACSHA1::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read_mp("VERIFY", _op).complete();
}

int
IPsecAuthHMACSHA1::initialize(ErrorHandler *)
{
  _drops = 0;
  return 0;
}


Packet *
IPsecAuthHMACSHA1::simple_action(Packet *p)
{
  SADataTuple * sa_data=(SADataTuple *)IPSEC_SA_DATA_REFERENCE_ANNO(p);
  unsigned int len;
  // compute HMAC
  len = SHA_DIGEST_LEN;

  if (_op == COMPUTE_AUTH) {
    unsigned char digest [SHA_DIGEST_LEN];
    HMAC(sa_data->Authentication_key,KEY_SIZE,(u_char*) p->data(),p->length(),digest,&len);
    WritablePacket *q = p->put(12);
    u_char *ah = ((u_char*)q->data())+q->length()-12;
    memmove(ah, digest, 12);
    return q;
  }
  else {
    const u_char *ah = p->data()+p->length()-12;
    unsigned char digest [SHA_DIGEST_LEN];

    HMAC(sa_data->Authentication_key,KEY_SIZE,(u_char*) p->data(),p->length()-12,digest,&len);
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
    //remove digest
    p->take(12);
    return p;
  }
}

String
IPsecAuthHMACSHA1::drop_handler(Element *e, void *)
{
  IPsecAuthHMACSHA1 *a = (IPsecAuthHMACSHA1 *)e;
  return String(a->_drops);
}

void
IPsecAuthHMACSHA1::add_handlers()
{
  add_read_handler("drops", drop_handler, 0);
}

#include "sha1_impl.cc"
#include "hmac.cc"

CLICK_ENDDECLS
EXPORT_ELEMENT(IPsecAuthHMACSHA1)
ELEMENT_MT_SAFE(IPsecAuthHMACSHA1)
