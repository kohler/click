#ifndef AIROINFO_HH
#define AIROINFO_HH

/*
 * =c
 * AiroInfo(INTERFACE-NAME)
 * =s Grid
 * =d
 *
 * This element supplies the Aironet card's statistics and information
 * to other elements.
 *
 * This element requires a BSD kernel with an Aironet driver that is
 * modified to support the required ioctls.
 * 
 * =a ToDevice */

#include <click/element.hh>
#include <click/etheraddress.hh>

/* OpenBSD 2.9 doesn't protect <net/if.h> from multiple inclusion, but
   later versions do */
#ifndef CLICK_NET_IF_H
#define CLICK_NET_IF_H
#include <net/if.h>
#endif

class AiroInfo : public Element {
  
public:
  
  AiroInfo();
  ~AiroInfo();
  
  const char *class_name() const		{ return "AiroInfo"; }

  AiroInfo *clone() const         { return new AiroInfo; }
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  /* If the card has signal strength and quality for sender E, return
   * true and place the information into DBM and QUALITY.  Else return
   * false */
  bool get_signal_info(const EtherAddress &e, int &dbm, int &quality);

  /* If the card has link-layer transmission statistics, return true
   * and place the information into DBM and QUALITY.  Else return
   * false */
  bool get_tx_stats(const EtherAddress &e, int &num_successful, int &num_failed);
  
  /* Clear all link-layer transmission statistics on the card */
  void clear_tx_stats();

private:

  int _fd;
  String _ifname;

  struct ifreq _ifr;
};

#endif
