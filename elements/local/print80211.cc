/*
 * print80211.{cc,hh} -- element prints IEEE 802.11 packet headers
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include "print80211.hh"
#include <click/glue.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#if __FreeBSD__ >= 4
# include <net/ethernet.h>
# include <net/if.h>
# if __FreeBSD__ >= 6
#  include <net80211/ieee80211.h>
# else
#  include <net/if_ieee80211.h>
# endif
#endif
CLICK_DECLS

Print80211::Print80211()
{
}

Print80211::~Print80211()
{
}

int
Print80211::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String label;
  bool timestamp = false;
  bool verbose = false;
  if (Args(conf, this, errh)
      .read_p("LABEL", label)
      .read("TIMESTAMP", timestamp)
      .read("VERBOSE", verbose)
      .complete() < 0)
    return -1;

  _label = label;
  _timestamp = timestamp;
  _verbose = verbose;
  return 0;
}

#if defined(__FreeBSD__) && __FreeBSD__ >= 4
static String
hex_string(unsigned i, bool add_x = false)
{
  char buf[100];
  snprintf(buf, sizeof(buf), "%s%x", (add_x ? "0x" : ""), i);
  return buf;
}

static void
print_data(StringAccum &s, bool /* verbose */, const uint8_t *buf, unsigned int buflen)
{
  if (buflen < 26) {
    s << "Data frame too short (have " << buflen << ", wanted at least 26)";
    return;
  }
  uint8_t fc1 = buf[1];
  bool to_ds = ((fc1 & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_TODS);
  bool from_ds = ((fc1 & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_FROMDS);
  EtherAddress a1(buf + 4);
  EtherAddress a2(buf + 10);
  EtherAddress a3(buf + 16);
  // see spec pg. 44, section 7.2.2 table 4.
  if (!to_ds && !from_ds)
    s << "SA " << a2.s() << " -> DA " << a1.s() << ", BSSID " << a3.s();
  else if (!to_ds && from_ds)
    s << "BSSID " << a2.s() << " -> DA " << a1.s() << ", SA " << a3.s();
  else if (to_ds && !from_ds)
    s << "SA " << a2.s() << " -> BSSID " << a1.s() << ", DA " << a3.s();
  else {
    EtherAddress a4(buf + 24);
    s << "TA " << a2.s() << " -> RA " << a1.s() << ", DA " << a3.s() << ", SA " << a4.s();
  }
  char sbuf[100];
  snprintf(sbuf, sizeof(sbuf), "  Seq: %02x %02x\n", (unsigned) buf[22], (unsigned) buf[23]);
  s << sbuf;
}

static void
print_mgmt(StringAccum &s, bool /* verbose */, const uint8_t *buf, unsigned int buflen)
{
  if (buflen < 24) {
    s << "Management frame too short (have " << buflen << ", wanted at least 24)\n";
    return;
  }
  EtherAddress dst(buf + 4);
  EtherAddress src(buf + 10);
  EtherAddress bssid(buf + 16);
  s << "DA: " << dst.s() << "  SA: " << src.s() << "  BSSID: " << bssid.s();
}

static void
print_ctl(StringAccum &s, bool /* verbose */, const uint8_t *buf, unsigned int /* buflen */)
{
  uint8_t fc0 = buf[0];
  switch (fc0 & IEEE80211_FC0_SUBTYPE_MASK) {
  case IEEE80211_FC0_SUBTYPE_RTS: {
    EtherAddress ra(buf + 4);
    EtherAddress ta(buf + 10);
    s << "RA: " <<  ra.s() << "  TA: " << ta.s();
    break;
  }
  case IEEE80211_FC0_SUBTYPE_CTS:
  case IEEE80211_FC0_SUBTYPE_ACK: {
    EtherAddress ra(buf + 4);
    s << "RA: " << ra.s().c_str();
    break;
  }
  case IEEE80211_FC0_SUBTYPE_PS_POLL: {
    s << "AID is Duration  ";
    EtherAddress bssid(buf + 4);
    EtherAddress ta(buf + 10);
    s << "BSSID: " <<  bssid.s() << "  TA: " << ta.s();
    break;
  }
  case IEEE80211_FC0_SUBTYPE_CF_END:
  case IEEE80211_FC0_SUBTYPE_CF_END_ACK: {
    EtherAddress ra(buf + 4);
    EtherAddress bssid(buf + 10);
    s << "RA: " << ra.s() << "  BSSID: " << bssid.s();
    break;
  }
  default:
    s << "Unknown control frame subtype " << hex_string(fc0 & IEEE80211_FC0_SUBTYPE_MASK);
  }
}

static const char *
frame_type(const uint8_t *fctl)
{
  static String s;
  s = "";
  uint8_t fc0 = fctl[0];
  unsigned type = IEEE80211_FC0_TYPE_MASK & fc0;
  unsigned subtype = IEEE80211_FC0_SUBTYPE_MASK & fc0;

  switch (type) {
  case IEEE80211_FC0_TYPE_MGT:
    s = "Management, ";
    switch (subtype) {
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
    default: s += "Unknown subtype " + hex_string(subtype);
    }
    break;
  case IEEE80211_FC0_TYPE_CTL:
    s = "Control, ";
    switch (subtype) {
    case IEEE80211_FC0_SUBTYPE_PS_POLL: s += "PS Poll"; break;
    case IEEE80211_FC0_SUBTYPE_RTS: s += "RTS"; break;
    case IEEE80211_FC0_SUBTYPE_CTS: s += "CTS"; break;
    case IEEE80211_FC0_SUBTYPE_ACK: s += "ACK"; break;
    case IEEE80211_FC0_SUBTYPE_CF_END: s += "CF End"; break;
    case IEEE80211_FC0_SUBTYPE_CF_END_ACK: s += "CF End ACK"; break;
    default: s += "Unknown subtype " + hex_string(subtype);
    }
    break;
  case IEEE80211_FC0_TYPE_DATA:
    s = "Data, ";
    switch (subtype) {
    case IEEE80211_FC0_SUBTYPE_DATA: s += "Real data"; break;
    case IEEE80211_FC0_SUBTYPE_CF_ACK: s += "CF ACK"; break;
    case IEEE80211_FC0_SUBTYPE_CF_POLL: s += "CF poll"; break;
    case IEEE80211_FC0_SUBTYPE_CF_ACPL: s += "CF ACPL"; break;
    case IEEE80211_FC0_SUBTYPE_NODATA: s += "No data"; break;
    case IEEE80211_FC0_SUBTYPE_CFACK: s += "CFACK"; break;
    case IEEE80211_FC0_SUBTYPE_CFPOLL: s += "CFPOLL"; break;
    case IEEE80211_FC0_SUBTYPE_CF_ACK_CF_ACK: s += "CF ACK of ACK"; break;
    default: s += "Unknown subtype " + hex_string(subtype);
    }
    break;
  default:
    s = "Unknown type " + hex_string(type);
  }
  return s.c_str();
}
#endif // __FreeBSD__ version check

Packet *
Print80211::simple_action(Packet *p)
{
  StringAccum sa;
  sa << _label;
  if (_label)
    sa << ": ";
  if (_timestamp)
    sa << p->timestamp_anno() << ": ";

  char sbuf[100];
  snprintf(sbuf, sizeof(sbuf), "%4d | ", p->length());
  sa << sbuf;

#if defined(__FreeBSD__) && __FreeBSD__ >= 4
  ieee80211_frame *frame = (ieee80211_frame *) p->data();

  // print type, try to put together 802.11 headers...
  sa << "Frame type: " << frame_type(frame->i_fc) << "  ";

  unsigned fc0 = frame->i_fc[0];
  if (_verbose) {
    unsigned fc1 = frame->i_fc[1];
    unsigned dur0 = ntohs(frame->i_dur);
    snprintf(sbuf, sizeof(sbuf), "Frame Control: %02x %02x  ", fc0, fc1);
    sa << sbuf;
    snprintf(sbuf, sizeof(sbuf), "Duration: %04x  ", dur);
    sa << sbuf;
  }

  unsigned type = fc0 & IEEE80211_FC0_TYPE_MASK;
  switch (type) {
  case IEEE80211_FC0_TYPE_DATA: print_data(sa, _verbose, p->data(), p->length()); break;
  case IEEE80211_FC0_TYPE_MGT: print_mgmt(sa, _verbose, p->data(), p->length()); break;
  case IEEE80211_FC0_TYPE_CTL: print_ctl(sa, _verbose, p->data(), p->length()); break;
  default:
    snprintf(sbuf, sizeof(sbuf),"x%x", type);
    sa << "Unknown 802.11 packet type " << sbuf;
  }
#endif

  click_chatter("%s", sa.c_str());
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Print80211)
ELEMENT_REQUIRES(userlevel)
