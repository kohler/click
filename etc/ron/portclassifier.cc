/*
 * switch.{cc,hh} -- element routes packets to one output of several
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "portswitch.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_tcp.h>

PortSwitch::PortSwitch()
  : Element(1,2)
{
  MOD_INC_USE_COUNT;
}

PortSwitch::~PortSwitch()
{
  MOD_DEC_USE_COUNT;
}

PortSwitch *
PortSwitch::clone() const
{
  return new PortSwitch;
}
/*
void
PortSwitch::notify_noutputs(int n)
{
  _noutputs = n;
  set_noutputs(n);

}
*/
int
PortSwitch::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpUnsigned, "breakpoint", &_breakpoint,
		  0) < 0)
    return -1;
  return 0;
}

void
PortSwitch::push(int, Packet *p)
{
#if 0
  int stepping = 100;
  int base = 60000;

  unsigned int port=0;
  unsigned int sport, dport;
  const click_tcp *tcph= p->tcp_header();
  sport = ntohs(tcph->th_sport);
  dport = ntohs(tcph->th_dport);

  port = sport / stepping - base / stepping;
  if (port < 0)
    port = 0;
  if (port >= _noutputs)
    port = _noutputs-1;

  click_chatter("  sport: %d, dport: %d chose: %d", sport, dport, port);
#endif

  unsigned int sport;
  const click_tcp *tcph= p->tcp_header();
  sport = ntohs(tcph->th_sport);
  
  if (sport < _breakpoint) 
    output(0).push(p);
  else 
    output(1).push(p);

  return;
}

EXPORT_ELEMENT(PortSwitch)
ELEMENT_MT_SAFE(PortSwitch)
