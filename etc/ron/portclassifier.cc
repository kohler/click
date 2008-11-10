/*
 * portclassifier.{cc,hh} -- splits packets around a tcp source port number
 * Alexander Yip
 *
 * Copyright (c) 2002 MIT
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
#include "portclassifier.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/tcp.h>

PortClassifier::PortClassifier()
{
}

PortClassifier::~PortClassifier()
{
}

int
PortClassifier::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  String srcdst;
  if (cp_va_parse(conf, this, errh,
                  cpString, "srcdst", &srcdst,
		  cpUnsigned, "base", &_base,
                  cpUnsigned, "stepping", &_stepping,
		  cpEnd) < 0)
    return -1;

  if (srcdst == "SRC")
    _src = 1;
  else if (srcdst == "DST")
    _src = 0;
  else
    return -1;

  return 0;
}

void
PortClassifier::push(int, Packet *p)
{
  int port=0;
  unsigned int sport, dport;
  const click_tcp *tcph= p->tcp_header();
  sport = ntohs(tcph->th_sport);
  dport = ntohs(tcph->th_dport);

  if (_src)
    port = sport / _stepping - _base / _stepping;
  else
    port = dport / _stepping - _base / _stepping;

  if (port >= noutputs())
    port = noutputs()-1;

  //click_chatter("  sport: %d, dport: %d chose: %d", sport, dport, port);
  output(port).push(p);
  return;
}

EXPORT_ELEMENT(PortClassifier)
ELEMENT_MT_SAFE(PortClassifier)
