/*
 * frombpf.{cc,hh} -- element reads packets live from network via pcap
 * John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "frombpf.hh"
#include "confparse.hh"
#include "error.hh"

FromBPF::FromBPF()
{
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
