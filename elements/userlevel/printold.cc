/*
 * printold.{cc,hh} -- element prints ``old'' packet contents to system log
 * Douglas S. J. De Couto
 * based on print.{cc,hh}
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2008 Regents of the University of California
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
#include "printold.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
CLICK_DECLS

PrintOld::PrintOld()
{
}

PrintOld::~PrintOld()
{
}

int
PrintOld::configure(Vector<String> &conf, ErrorHandler* errh)
{
    _label = String();
    _bytes = 24;
    _thresh = 5;

    if (Args(conf, this, errh)
	.read_p("LABEL", _label)
	.read_p("AGE", _thresh)
	.read_p("MAXLENGTH", _bytes)
	.read("LENGTH", Args::deprecated, _bytes)
	.read("NBYTES", Args::deprecated, _bytes)
	.complete() < 0)
	return -1;

    return 0;
}

Packet *
PrintOld::simple_action(Packet *p)
{
  if (p->timestamp_anno().sec() == 0) {
    click_chatter("%s: packet timestamp not set", name().c_str());
    return p;
  }

  int bytes = _bytes;
  if (bytes < 0 || (int) p->length() < bytes)
      bytes = p->length();
  StringAccum sa(3*bytes + _label.length() + 55);
  if (sa.out_of_memory()) {
    click_chatter("no memory for PrintOld");
    return p;
  }

  Timestamp now = Timestamp::now();

  long age_s = tv_now.sec() - p->timestamp_anno().sec();
  long age_u = tv_now.usec() - p->timestamp_anno().usec();

  // skankyness...
  long age_ms = age_s * 1000 + age_u / 1000;

#if 1
  assert(sizeof(long) == sizeof(int));
  if (age_ms > _thresh)
    click_chatter("%s Now-FromDevice age is %d (FromDevice time: %p{timestamp}  dsec %ld  dusec %ld)",
		  name().c_str(), age_ms, &p->timestamp_anno(), age_s, age_u);
#endif

#if 1
  // see hack in fromdevice.cc
  struct timeval pcap_tv;
  pcap_tv.tv_sec = (long) p->user_anno_i(0);
  pcap_tv.tv_usec = (long) p->user_anno_i(1);
  if (pcap_tv.tv_sec == 0) {
    // click_chatter("%s pcap time not set", name().c_str());
  }
  else {
    long age2_s = p->timestamp_anno().tv_sec - pcap_tv.tv_sec;
    long age2_u = p->timestamp_anno().tv_usec - pcap_tv.tv_usec;
    long age2_ms = age2_s * 1000 + age2_u / 1000;
    if (age2_ms > _thresh)
      click_chatter("%s FromDevice-PCAP age is %d (PCAP time: %ld.%06ld  dsec %ld  dusec %ld)",
		    name().c_str(), age2_ms,
		    pcap_tv.tv_sec, pcap_tv.tv_usec,
		    age2_s, age2_u);
  }
#endif

  if (age_ms < _thresh)
    return p;

  // else print it...

  sa << _label;

  // sa.reserve() must return non-null; we checked capacity above
  int len;
  sprintf(sa.reserve(9), "(%5ld msecs) %4d | %n", age_ms, p->length(), &len);
  sa.adjust_length(len);

  char *buf = sa.data() + sa.length();
  int pos = 0;
  for (int i = 0; i < bytes; i++) {
    sprintf(buf + pos, "%02x", p->data()[i] & 0xff);
    pos += 2;
    if ((i % 4) == 3) buf[pos++] = ' ';
  }
  sa.adjust_length(pos);

  click_chatter("%s", sa.c_str());

  return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel false)
EXPORT_ELEMENT(PrintOld)
