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
#include <click/etheraddress.hh>
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

static void
print_data(u_int8_t *buf, unsigned int buflen, bool print_payload = false)
{
  printf("Frame Control: %02x %02x\n", (int) buf[0], (int) buf[1]);
  printf("Duration: %02x %02x\n", (int) buf[2], (int) buf[3]);
  EtherAddress a1(buf + 4);
  EtherAddress a2(buf + 10);
  EtherAddress a3(buf + 16);
  printf("A1: %s\n", a1.s().cc());
  printf("A2: %s\n", a2.s().cc());
  printf("A3: %s\n", a3.s().cc());
  printf("Seq: %02x %02x\n", (int) buf[22], (int) buf[23]);
  if (print_payload) {
    for (unsigned int i = 24; i < buflen; i += 8) {
      for (unsigned int j = i; j < buflen && j < i + 8; j++)
	printf("%02x ", (int) buf[j]);
      printf("\n");
    }
  }
}

static const char *
frame_type(u_int16_t fctl)
{
  static String s;
  s = "";
  u_int8_t *f = (u_int8_t *) &fctl;
  u_int8_t fc0 = f[0];

  switch (IEEE80211_FC0_TYPE_MASK & fc0) {
  case IEEE80211_FC0_TYPE_MGT:
    s = "Management, ";
    switch (IEEE80211_FC0_SUBTYPE_MASK & fc0) {
    case IEEE80211_FC0_SUBTYPE_ASSOC_REQ: s += "Association request"; break;
    case IEEE80211_FC0_SUBTYPE_ASSOC_RESP: s += "Association response"; break;  
    case IEEE80211_FC0_SUBTYPE_REASSOC_REQ: s += "Reassociation request"; break;
    case IEEE80211_FC0_SUBTYPE_REASSOC_RESP: s += "Reassociation response"; break;
    case IEEE80211_FC0_SUBTYPE_PROBE_REQ: s += "Probe request"; break;
    case IEEE80211_FC0_SUBTYPE_PROBE_RESP: s+= "Probe response"; break;
    case IEEE80211_FC0_SUBTYPE_BEACON: s += "Beacon"; break;
    case IEEE80211_FC0_SUBTYPE_ATIM: s += "ATIM"; break;
    case IEEE80211_FC0_SUBTYPE_DISASSOC: s += "Disassociation"; break;
    case IEEE80211_FC0_SUBTYPE_AUTH: s += "Authorization"; break;
    case IEEE80211_FC0_SUBTYPE_DEAUTH: s += "Deauthorization"; break;
    default: s += "Unknown";
    }
    break;
  case IEEE80211_FC0_TYPE_CTL:
    s = "Control, ";
    switch (IEEE80211_FC0_SUBTYPE_MASK & fc0) {
    case IEEE80211_FC0_SUBTYPE_PS_POLL: s += "PS Poll"; break;
    case IEEE80211_FC0_SUBTYPE_RTS: s += "RTS"; break;
    case IEEE80211_FC0_SUBTYPE_CTS: s += "CTS"; break;
    case IEEE80211_FC0_SUBTYPE_ACK: s += "ACK"; break;
    case IEEE80211_FC0_SUBTYPE_CF_END: s += "CF End"; break;
    case IEEE80211_FC0_SUBTYPE_CF_END_ACK: s += "CF End ACK"; break;
    default: s += "Unknown";
    }
    break;
  case IEEE80211_FC0_TYPE_DATA:
    s = "Data, ";
    switch (IEEE80211_FC0_SUBTYPE_MASK & fc0) {
    case IEEE80211_FC0_SUBTYPE_DATA: s += "Real data"; break;
    case IEEE80211_FC0_SUBTYPE_CF_ACK: s += "CF ACK"; break;
    case IEEE80211_FC0_SUBTYPE_CF_POLL: s += "CF poll"; break;
    case IEEE80211_FC0_SUBTYPE_CF_ACPL: s += "CF ACPL"; break;
    case IEEE80211_FC0_SUBTYPE_NODATA: s += "No data"; break;
    case IEEE80211_FC0_SUBTYPE_CFACK: s += "CFACK"; break;
    case IEEE80211_FC0_SUBTYPE_CFPOLL: s += "CFPOLL"; break;
    case IEEE80211_FC0_SUBTYPE_CF_ACK_CF_ACK: s += "CF ACK of ACK"; break;
    default: s += "Unknown";
    }
    break;
  default:
    s = "Unknown";
  }
  return s.cc();
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
  
  struct an_rxframe *frame = (struct an_rxframe *) p->data();
  
  // copy the 802.11 MAC frame into a static buffer, to get rid of the
  // gap, as done in the an driver
  static u_int8_t buf[2048];
  u_int16_t fc0 = frame->an_frame_ctl & 0xff;
  u_int16_t fc1 = frame->an_frame_ctl >> 8;
  int ieee80211_header_len = sizeof(struct ieee80211_frame);
  assert(ieee80211_header_len == 24);
  if ((fc1 & IEEE80211_FC1_DIR_TODS) && 
      (fc1 & IEEE80211_FC1_DIR_FROMDS))
    ieee80211_header_len += ETHER_ADDR_LEN;
  unsigned int len = frame->an_rx_payload_len + ieee80211_header_len;
  if (len > sizeof(buf)) {
    printf("\tFrame too big (%d > %d)\n", len, sizeof(buf));
    return p;
  }

  memcpy(buf, &frame->an_frame_ctl, ieee80211_header_len);

  // mind the gap!
  memcpy(buf + ieee80211_header_len, 
	 ((u_int8_t *) frame) + sizeof(struct an_rxframe) + frame->an_gaplen,
	 frame->an_rx_payload_len);

  int buflen = ieee80211_header_len + frame->an_rx_payload_len; 

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

  printf("\tExpected %d PSDU octets, got %d\n", len_octets, buflen);
  printf("\tAiro payload length: %d\n", (int) frame->an_rx_payload_len);
  printf("\tIEEE header length: %d\n", ieee80211_header_len);
  printf("\tGap length: %d\n", (int) frame->an_gaplen);

  // print type, try to put together 802.11 headers...
  printf("\tFrame type: %s\n", frame_type(frame->an_frame_ctl));

  // spit out just the 802.11 frame
  WritablePacket *q = Packet::make(buf, buflen);
  p->kill();
  return q;

#if 0  
  // try to set IP header anno and send out data packets
  if (((fc0 & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA) &&
      ((fc0 & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_DATA)) {
    print_data(buf, buflen, false);
    int eth_type_off = sizeof(struct an_rxframe) + frame->an_gaplen + 8;
    u_int16_t *tc = (u_int16_t *) (p->data() + eth_type_off);
    if (ntohs(*tc) == 0x0800) {
      // maybe this really is an IP packet....
      struct click_ip *ip = (struct click_ip *) (tc + 1);
      p->set_ip_header(ip, ntohs(ip->ip_len));
      return p;
    }
  }
  p->kill();
  return 0;
#endif
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintAiro)
ELEMENT_REQUIRES(userlevel false)
