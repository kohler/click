/*
 * printairo.{cc,hh} -- element prints Aironet packet headers
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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

#include <math.h>
#include <click/config.h>
#include "printairo.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#ifdef CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/sched.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_ieee80211.h>

// stolen from FreeBSD /usr/src/sys/dev/an/if_aironet_ieee.h
/*
 * Receive frame structure.
 */
struct an_rxframe {
	u_int32_t		an_rx_time;		/* 0x00 */
	u_int16_t		an_rx_status;		/* 0x04 */
	u_int16_t		an_rx_payload_len;	/* 0x06 */
	u_int8_t		an_rsvd0;		/* 0x08 */
	u_int8_t		an_rx_signal_strength;	/* 0x09 */
	u_int8_t		an_rx_rate;		/* 0x0A */
	u_int8_t		an_rx_chan;		/* 0x0B */
	u_int8_t		an_rx_assoc_cnt;	/* 0x0C */
	u_int8_t		an_rsvd1[3];		/* 0x0D */
	u_int8_t		an_plcp_hdr[4];		/* 0x10 */
	u_int16_t		an_frame_ctl;		/* 0x14 */
	u_int16_t		an_duration;		/* 0x16 */
	u_int8_t		an_addr1[6];		/* 0x18 */
	u_int8_t		an_addr2[6];		/* 0x1E */
	u_int8_t		an_addr3[6];		/* 0x24 */
	u_int16_t		an_seq_ctl;		/* 0x2A */
	u_int8_t		an_addr4[6];		/* 0x2C */
	u_int8_t		an_gaplen;		/* 0x32 */
} __attribute__((packed));

PrintAiro::PrintAiro()
  : Element(1, 1)
{
  MOD_INC_USE_COUNT;
}

PrintAiro::~PrintAiro()
{
  MOD_DEC_USE_COUNT;
}

PrintAiro *
PrintAiro::clone() const
{
  return new PrintAiro;
}

int
PrintAiro::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String label;
  bool timestamp = false;
  if (cp_va_parse(conf, this, errh,
		  cpOptional,
		  cpString, "label", &label,
		  cpKeywords,
		  "TIMESTAMP", cpBool, "print packet timestamps?", &timestamp,
		  cpEnd) < 0)
    return -1;
  
  _label = label;
  _timestamp = timestamp;
  return 0;
}

static char bits[9];

static char *
bit_string(u_int8_t b) 
{
  bits[8] = 0;
  for (int i = 0; i < 8; i++) {
    int mask = 1 << i;
    if (mask & b) 
      bits[i] = '1';
    else
      bits[i] = '.';
  }
  return bits;
}

static char *
frame_type(u_int16_t fctl)
{
  u_int8_t *f = (u_int8_t *) &fctl;
  switch (IEEE80211_FC0_TYPE_MASK & f[0]) {
  case IEEE80211_FC0_TYPE_MGT:
    return "Management";
    break;
  case IEEE80211_FC0_TYPE_CTL:
    return "Control";
    break;
  case IEEE80211_FC0_TYPE_DATA:
    return "Data";
    break;
  default:
    return "Unknown";
  }  
}

Packet *
PrintAiro::simple_action(Packet *p)
{
  StringAccum sa;
  sa << _label;
  if (_label)
    sa << ": ";
  if (_timestamp)
    sa << p->timestamp_anno() << ": ";
  printf("%s %4d bytes\n", sa.cc(), p->length());
  
  struct an_rxframe *frame = (an_rxframe *) p->data();
  
  printf("\tRSSI: %d\n", (int) frame->an_rx_signal_strength);
  int r = frame->an_rx_rate / 2;
  bool print5 = (r * 2 < frame->an_rx_rate);
  printf("\tRate: %d%s Mbps\n", r, print5 ? ".5" : "");
  printf("\tChan: %d\n", (int) frame->an_rx_chan);
  
  if (frame->an_plcp_hdr[0] == 0x0a)
    printf("\tPLCP.Signal: 1 Mbps DBPSK\n");
  else if (frame->an_plcp_hdr[0] == 0x14)
    printf("\tPLCP.Signal: 2 Mbps DQPSK\n");
  else if (frame->an_plcp_hdr[0] == 0x37)
    printf("\tPLCP.Signal: 5.5 Mbps\n");
  else if (frame->an_plcp_hdr[0] == 0x6e)
    printf("\tPLCP.Signal: 11 Mbps\n");
  else
    printf("unknown\n");
  
  u_int8_t svc = frame->an_plcp_hdr[1];
  printf("\tPLCP.Service: locked_clocks=%d modulation=%s length_extension=%d (%s)\n",
	 svc & 4 ? 1 : 0, svc & 8 ? "PBCC" : "CCK", svc & 128 ? 1 : 0, bit_string(svc));

  int len_usecs = (frame->an_plcp_hdr[3] << 8) | frame->an_plcp_hdr[2];
  printf("\tPLCP.Length: %d microseconds\n", len_usecs);

  int len_octets; // see 802.11b sec 18.2.3.5
  switch (frame->an_rx_rate) {
  case 2:
    len_octets = len_usecs / 8;
    break;
  case 4:
    len_octets = len_usecs / 4;
    break;
  case 11:
    if (svc & 8)  // PBCC
      len_octets = (int) floor((len_usecs * 5.5 / 8) - 1);
    else // CCK
      len_octets = (int) floor(len_usecs * 5.5 / 8);
    break;
  case 22:
    if (svc & 8) // PBCC
      len_octets = (int) floor((len_usecs * 11 / 8) - 1);
    else  // CCK
      len_octets = (int) floor(len_usecs * 11 / 8);
    if (svc & 128)
      len_octets--;
    break;
  default:
    len_octets = -1;
  }

  // subtract Aironet-specific stuff from hdr, add in FCS bytes
  // fuck, to do this right i need to demux on what sort of 802.11 frame it actually is...
  int num_octets_got = p->length() - 0x14 - 1 + 4; 

  printf("\tExpected %d PSDU octets, got %d\n", len_octets, num_octets_got);
  printf("\tAiro payload length: %d\n", (int) frame->an_rx_payload_len);
  printf("\tGap length: %d\n", (int) frame->an_gaplen);

  // print type, try to put together 802.11 headers...
  printf("\tFrame type: %s\n", frame_type(frame->an_frame_ctl));

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintAiro)
ELEMENT_REQUIRES(userlevel)
