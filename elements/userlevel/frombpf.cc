/*
 * frombpf.{cc,hh} -- element reads packets live from network via pcap
 * John Jannotti
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
#include "frombpf.hh"
#include <click/confparse.hh>
#include <click/error.hh>

FromBPF::FromBPF()
{
  MOD_INC_USE_COUNT;
}

FromBPF::~FromBPF()
{
  MOD_DEC_USE_COUNT;
}

FromBPF *
FromBPF::clone() const
{
  return new FromBPF;
}

int
FromBPF::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String ifname;
  bool promisc;
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &ifname,
		  cpOptional,
		  cpBool, "be promiscuous", &promisc,
		  cpEnd) < 0)
    return -1;
  return errh->error("FromBPF is deprecated; use FromDevice instead");
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(FromBPF)
