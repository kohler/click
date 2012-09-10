#ifndef CLICK_AIROINFO_HH
#define CLICK_AIROINFO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#ifdef __linux__
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#else
/* OpenBSD 2.9 doesn't protect <net/if.h> from multiple inclusion, but
   later versions do */
#ifndef CLICK_NET_IF_H
#define CLICK_NET_IF_H
#include <net/if.h>
#endif
#endif
CLICK_DECLS

/*
 * =c
 * AiroInfo(DEVNAME)
 * =s Grid
 * =d
 *
 * This element supplies the Aironet card's statistics and information
 * to other elements.
 *
 * This element requires a BSD kernel with an Aironet driver that is
 * modified to support the required ioctls.
 *
 * OR, this element will work under linux using the Wireless
 * Extensions, but the wireless card driver will still need to be
 * modified to automatically add entries to the ``spy list''.
 *
 * =a ToDevice */

class AiroInfo : public Element {

public:

  AiroInfo() CLICK_COLD;
  ~AiroInfo() CLICK_COLD;

  const char *class_name() const		{ return "AiroInfo"; }
  const char *port_count() const		{ return PORTS_0_0; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  /* If the card has signal strength and quality for sender E, return
   * true and place the information into DBM and QUALITY.  Else return
   * false */
  bool get_signal_info(const EtherAddress &e, int &dbm, int &quality);

  /* If the card has link-layer transmission statistics, return true
   * and place the information into DBM and QUALITY.  Else return
   * false */
  bool get_tx_stats(const EtherAddress &e, int &num_successful, int &num_failed);

  /* If possible, place the card's background noise measurements (in
     dBm) into the arguments, and return true.  Else return false */
  bool get_noise(int &max_over_sec, int &avg_over_minute, int &max_over_minute);

  /* Clear all link-layer transmission statistics on the card */
  void clear_tx_stats();

private:

  int _fd;
  String _ifname;

#ifdef __linux__
  struct iwreq _ifr;
  struct ifreq _ifr2;
#else
  struct ifreq _ifr;
#endif

};

CLICK_ENDDECLS
#endif
