/*
 * tobpf.{cc,hh} -- element writes packets to network via pcap library
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
#include "tobpf.hh"
#include "confparse.hh"
#include "error.hh"

ToBPF::ToBPF()
{
}

ToBPF *
ToBPF::clone() const
{
  return new ToBPF;
}

int
ToBPF::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String ifname;
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &ifname,
		  0) < 0)
    return -1;
  return errh->error("ToBPF has been deprecated; use ToDevice instead");
}

EXPORT_ELEMENT(ToBPF)
