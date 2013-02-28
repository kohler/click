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

#include <click/config.h>
#include "printairo.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#ifdef CLICK_USERLEVEL
#include <math.h>
#endif

CLICK_DECLS

// stolen from FreeBSD /usr/src/sys/dev/an/if_anreg.h
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
{
}

PrintAiro::~PrintAiro()
{
}

int
PrintAiro::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String label;
  bool timestamp = false;
  bool quiet = false;
  bool verbose = false;
  if (Args(conf, this, errh)
      .read_p("LABEL", label)
      .read("TIMESTAMP", timestamp)
      .read("QUIET", quiet)
      .read("VERBOSE", verbose)
      .complete() < 0)
    return -1;

  _label = label;
  _timestamp = timestamp;
  _quiet = quiet;
  _verbose = verbose;
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


// stolen from FreeBSD /usr/include/net/if_ieee80211.h
#define	IEEE80211_FC1_DIR_TODS			0x01	/* STA->AP  */
#define	IEEE80211_FC1_DIR_FROMDS		0x02	/* AP ->STA */

Packet *
PrintAiro::simple_action(Packet *p)
{
  struct an_rxframe *frame = (struct an_rxframe *) p->data();

  if (p->length() < sizeof(*frame)) {
    click_chatter("%s: Packet too short to hold Aironet header", name().c_str());
    return 0;
  }

  // copy the 802.11 MAC frame into a static buffer, to get rid of the
  // gap, as done in the an driver
  static u_int8_t buf[2048];
  u_int16_t fc1 = frame->an_frame_ctl >> 8;
  int ieee80211_header_len = 24;
  if ((fc1 & IEEE80211_FC1_DIR_TODS) &&
      (fc1 & IEEE80211_FC1_DIR_FROMDS))
    ieee80211_header_len += 6; // there is a 4th MAC address in frame
  unsigned int len = frame->an_rx_payload_len + ieee80211_header_len;
  if (len > sizeof(buf)) {
    click_chatter("%s: Frame too big to copy into buffer (%d > %d)\n",
		  name().c_str(), len, (int)sizeof(buf));
    return 0;
  }

  memcpy(buf, &frame->an_frame_ctl, ieee80211_header_len);

  // mind the gap!
  memcpy(buf + ieee80211_header_len,
	 ((u_int8_t *) frame) + sizeof(struct an_rxframe) + frame->an_gaplen,
	 frame->an_rx_payload_len);

  if (!_quiet) {
    StringAccum sa;
    if (!_quiet) {
      if (_label)
	sa << _label;
      else
	sa << name();
      sa << ": ";
      if (_timestamp)
	sa << p->timestamp_anno() << ": ";

      int r = frame->an_rx_rate / 2;
      bool print5 = (r * 2 < frame->an_rx_rate);
      char info[1024];
      snprintf(info, sizeof(info), "%s%4d | RSSI: %d  Rate: %d%s Mbps   Chan: %d",
	       sa.c_str(), p->length(), (int) frame->an_rx_signal_strength,
	       r, print5 ? ".5" : "", (int) frame->an_rx_chan);
      click_chatter("%s", info);

      if (_verbose) {
	int plcp_rate = 0;
	switch (frame->an_plcp_hdr[0]) {
	case 0x0a: click_chatter("\tPLCP.Signal: 1 Mbps DBPSK\n"); plcp_rate = 2; break;
	case 0x14: click_chatter("\tPLCP.Signal: 2 Mbps DQPSK\n"); plcp_rate = 4; break;
	case 0x37: click_chatter("\tPLCP.Signal: 5.5 Mbps\n"); plcp_rate = 11; break;
	case 0x6e: click_chatter("\tPLCP.Signal: 11 Mbps\n"); plcp_rate = 22; break;
	default: click_chatter("PLCP.Signal value not recognized \n");
	}
	if (plcp_rate > 0 && plcp_rate != frame->an_rx_rate)
	  click_chatter("\tWarning: PLCP signal rate does not match rate provided by adapter!");

	u_int8_t svc = frame->an_plcp_hdr[1];
	click_chatter("\tPLCP.Service: locked_clocks=%d modulation=%s length_extension=%d (%s)\n",
	       svc & 4 ? 1 : 0, svc & 8 ? "PBCC" : "CCK", svc & 128 ? 1 : 0, bit_string(svc));

	int len_usecs = (frame->an_plcp_hdr[3] << 8) | frame->an_plcp_hdr[2];
	click_chatter("\tPLCP.Length: %d microseconds\n", len_usecs);

#ifdef CLICK_USERLEVEL
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
	click_chatter("\tExpected %d PSDU octets, got %d\n", len_octets, len);
#endif
	click_chatter("\tAiro payload length: %d\n", (int) frame->an_rx_payload_len);
	click_chatter("\tIEEE header length: %d\n", ieee80211_header_len);
	click_chatter("\tGap length: %d\n", (int) frame->an_gaplen);
      }
    }
  }

  // spit out the 802.11 frame
  // XXX alternative: instead of making new buffer, copy data over in current packet
  // XXX or, pass airo packet out 0, and push 802.11 packet to 1.
  p->kill();
  WritablePacket *q = Packet::make(buf, len);

  return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintAiro)
ELEMENT_REQUIRES(userlevel)
