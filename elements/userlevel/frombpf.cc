/*
 * frombpf.{cc,hh} -- element reads packets live from network via pcap
 * John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
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
