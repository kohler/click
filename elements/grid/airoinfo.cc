/*
 * airoinfo.{cc,hh} -- Access Aironet statistics
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include <cstddef>
#include "airoinfo.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
#include <click/etheraddress.hh>
#include "grid.hh"
#include <cmath>
#include <sys/ioctl.h>

#define ANLLFAIL
#define ANCACHE

#ifdef __OpenBSD__
#define FUCKED
#ifdef FUCKED
#include "/usr/src/sys/dev/ic/anvar.h"
#else
#include <dev/ic/anvar.h>
#endif
#endif

#ifdef __linux__
#include <linux/wireless.h>
#ifdef IW_MAX_SPY
#undef IW_MAX_SPY
#define IW_MAX_SPY 40 /* more fuckation -- this constant must be the same across all the drivers as well */
#endif
/* bullshit problem: dealing with different headers between the
   application cross-compile environment and the kernel cross-compile
   environment */
#endif

CLICK_DECLS

AiroInfo::AiroInfo() : 
  Element(0, 0), _fd(-1)
{
  MOD_INC_USE_COUNT;
}

AiroInfo::~AiroInfo()
{
  MOD_DEC_USE_COUNT;
  if (_fd > -1)
    close(_fd);
}

int
AiroInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{

  int res = cp_va_parse(conf, this, errh,
			cpString, "device name", &_ifname,
			0);
  return res;
}

int
AiroInfo::initialize(ErrorHandler *errh)
{
  memset(&_ifr, 0, sizeof(_ifr));
  strncpy(_ifr.ifr_name, _ifname.cc(), sizeof(_ifr.ifr_name));
  _ifr.ifr_name[sizeof(_ifr.ifr_name) - 1] = 0;

  _fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (_fd < 0) 
    return errh->error("Unable to open socket to %s device", _ifr.ifr_name);
  
  return 0;
}


#ifdef __OpenBSD__
bool
AiroInfo::get_signal_info(const EtherAddress &e, int &dbm, int &quality)
{
  struct an_req areq;
  memset(&areq, 0, sizeof(areq));
  
  areq.an_len = AN_MAX_DATALEN;
  areq.an_type = AN_RID_READ_CACHE;
  
  
  /* due to AN_MAX_DATALEN = 512 16-bit vals, we could only ever get
     ~56 entries from the card's cache.  however, since the current
     driver mod has only 30 entries, that's cool... but this could be
     a problem in big (e.g. > 30 nodes) networks... */
  _ifr.ifr_data = (char *) &areq;
  int res = ioctl(_fd, SIOCGAIRONET, &_ifr);
  if (res == -1) {
    click_chatter("AiroInfo: ioctl(SIOCGAIRONET) error when reading signal cache: %s\n", 
		  strerror(errno));
    return false;
  }
  
  int *num_entries = (int *) &areq.an_val;
  char *p = (char *) &areq.an_val;
  p += sizeof(int);
  struct an_sigcache *entries = (struct an_sigcache *) p;
  for (int i = 0; i < *num_entries; i++) {
    if (e == EtherAddress((unsigned char *) entries[i].macsrc)) {
      dbm = entries[i].signal;
      quality = entries[i].quality;
      return true;
    }
  }

  return false;
}

bool
AiroInfo::get_tx_stats(const EtherAddress &e, int &num_successful, int &num_failed)
{
  struct an_req areq;
  memset(&areq, 0, sizeof(areq));
  
  areq.an_len = AN_MAX_DATALEN;
  areq.an_type = AN_RID_READ_LLFAIL;
  
  _ifr.ifr_data = (char *) &areq;
  int res = ioctl(_fd, SIOCGAIRONET, &_ifr);
  if (res == -1) {
    click_chatter("AiroInfo: ioctl(SIOCGAIRONET) error when reading tx stats cache: %s\n", 
		  strerror(errno));
    return false;
  }
  
  int *num_entries = (int *) &areq.an_val;
  char *p = (char *) &areq.an_val;
  p += sizeof(int);
  struct an_llfailcache *entries = (struct an_llfailcache *) p;
  for (int i = 0; i < *num_entries; i++) {
    if (e == EtherAddress((unsigned char *) entries[i].macdst)) {
      num_failed = entries[i].num_fail;
      num_successful = entries[i].num_succeed;
      return true;
    }
  }
  return false;
}

bool
AiroInfo::get_noise(int &avg_over_sec, int &avg_over_minute, int &max_over_minute)
{
  struct an_req areq;
  memset(&areq, 0, sizeof(areq));

  areq.an_len = AN_MAX_DATALEN;
  areq.an_type = AN_RID_STATUS;
  
  _ifr.ifr_data = (char *) &areq;
  int res = ioctl(_fd, SIOCGAIRONET, &_ifr);
  if (res == -1) {
    click_chatter("AiroInfo: ioctl(SIOCGAIRONET) error when reading noise from status struct: %s\n", 
		  strerror(errno));
    return false;
  }
  
  // noise info from Marco Molteni (molter@tin.it)
  // u_int8_t                an_noise_prev_sec_pc;   /* 0x7A */
  // u_int8_t                an_noise_prev_sec_db;   /* 0x7B */
  // u_int8_t                an_avg_noise_prev_min_pc;       /* 0x7C */
  // u_int8_t                an_avg_noise_prev_min_db;       /* 0x7D */
  // u_int8_t                an_max_noise_prev_min_pc;       /* 0x7E */
  // u_int8_t                an_max_noise_prev_min_db;       /* 0x7F */

  u_int8_t *base = (u_int8_t *) _ifr.ifr_data;
  u_int8_t *u8 = base + 0x7B;
  avg_over_sec = *u8;

  u8 = base + 0x7D;
  avg_over_minute = *u8;

  u8 = base + 0x7F;
  max_over_minute = *u8;

  return true;
}
#endif

#ifdef __linux__
bool
AiroInfo::get_signal_info(const EtherAddress &e, int &dbm, int &quality)
{
  char buf[(sizeof(struct iw_quality) + sizeof(struct sockaddr)) * IW_MAX_SPY];

  _ifr.u.data.pointer = buf;
  _ifr.u.data.length = 0;
  _ifr.u.data.flags = 0;
  int res = ioctl(_fd, SIOCGIWSPY, &_ifr);
  if (res == -1) {
    click_chatter("AiroInfo: ioctl(SIOCGIWSPY) error when reading signal info: %s\n", 
		  strerror(errno));
    return false;
  }

  int n = _ifr.u.data.length;

  for (int i = 0; i < n; i++) {
    struct sockaddr *sa = (struct sockaddr *) (buf + i * sizeof(struct sockaddr));
    if (e == EtherAddress((unsigned char *) &sa->sa_data)) {
      struct iw_quality *q = (struct iw_quality *) (buf + n*sizeof(struct sockaddr) + i*sizeof(struct iw_quality));
      dbm = ((int) q->level) - 256;
      quality = q->qual;
      return true;
    }
  }

  return false;
}

bool
AiroInfo::get_tx_stats(const EtherAddress &, int &, int &)
{
  return false;
}

bool
AiroInfo::get_noise(int &, int &, int &)
{
  return false;
}
#endif


#if !defined(__linux__) && !defined(__OpenBSD__)
bool
AiroInfo::get_signal_info(const EtherAddress &, int &, int &)
{
  return false;
}

bool
AiroInfo::get_tx_stats(const EtherAddress &, int &, int &)
{
  return false;
}

bool
AiroInfo::get_noise(int &, int &, int &)
{
  return false;
}


#endif /* !__linux__ && !__OpenBSD__ */




void
AiroInfo::clear_tx_stats()
{
#ifdef __OpenBSD__
  struct an_req areq;
  memset(&areq, 0, sizeof(areq));
  areq.an_len = 0;
  areq.an_type = AN_RID_ZERO_LLFAIL;
  
  _ifr.ifr_data = (char *) &areq;
  int res = ioctl(_fd, SIOCGAIRONET, &_ifr);
  if (res == -1) {
    click_chatter("AiroInfo: ioctl(SIOCGAIRONET) error when resetting tx stats cache: %s\n",
		  strerror(errno));
  }
#endif
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AiroInfo)
