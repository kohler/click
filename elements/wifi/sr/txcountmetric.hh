#ifndef TXCOUNTMETRIC_HH
#define TXCOUNTMETRIC_HH
#include <click/element.hh>
#include "linkmetric.hh"
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include "ettstat.hh"
#include <elements/wifi/bitrate.hh>
CLICK_DECLS

/*
 * =c
 * TXCountMetric(LinkStat, LinkStat)
 * =s Wifi
 * The Estimated Transmission Count metric (ETX).
 * =a ETTMetric
 */



inline unsigned txcount_metric(int ack_prob, int data_prob, int data_rate) 
{
  
  if (!ack_prob || ! data_prob) {
    return 0;
  }
  int retries = 100 * 100 * 100 / (ack_prob * data_prob) - 100;
  unsigned low_usecs = calc_usecs_wifi_packet(1500, data_rate, retries/100);
  unsigned high_usecs = calc_usecs_wifi_packet(1500, data_rate, (retries/100) + 1);

  unsigned diff = retries % 100;
  unsigned average = (diff * high_usecs + (100 - diff) * low_usecs) / 100;
  return average;

}

class ETTStat;

class TXCountMetric : public LinkMetric {
  
public:


  TXCountMetric();
  ~TXCountMetric();
  const char *class_name() const { return "TXCountMetric"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);

  void *cast(const char *);

  static String read_stats(Element *xf, void *);

  void update_link(IPAddress from, IPAddress to, 
		   Vector<RateSize> rs, 
		   Vector<int> fwd, Vector<int> rev, 
		   uint32_t seq);

private:
  class LinkTable *_link_table;

};

CLICK_ENDDECLS
#endif
