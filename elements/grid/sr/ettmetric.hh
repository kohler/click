#ifndef ETTMETRIC_HH
#define ETTMETRIC_HH
#include <click/element.hh>
#include "linkmetric.hh"
#include <click/hashmap.hh>
#include <elements/grid/linktable.hh>
CLICK_DECLS

/*
 * =c
 * ETTMetric(LinkStat, LinkStat)
 * =s Grid
 * =io
 * None
 * =d
 *
 */

class IPOrderedPair {
public:
  IPAddress _a;
  IPAddress _b;
  
  IPOrderedPair() { }
  IPOrderedPair(IPAddress a, IPAddress b) {
    if (a.addr() > b.addr()) {
      _a = b;
      _b = a;
    } else {
      _a = a;
      _b = b;
    }
  }
  IPOrderedPair(const IPOrderedPair &p) : _a(p._a), _b(p._b) { }

  bool first(IPAddress x) {
    return (_a == x);
  }

  inline bool
  operator==(IPOrderedPair other)
  {
    return (other._a == _a && other._b == _b);
  }

};


inline unsigned
hashcode(IPOrderedPair p) 
{
  return hashcode(p._a) + hashcode(p._b);
}
class ETTStat;

class ETTMetric : public LinkMetric {
  
public:

  ETTMetric();
  ~ETTMetric();
  const char *class_name() const { return "ETTMetric"; }
  const char *processing() const { return AGNOSTIC; }
  ETTMetric *clone()  const { return new ETTMetric; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }
  void take_state(Element *, ErrorHandler *);
  void add_handlers();

  void *cast(const char *);

  static String read_stats(Element *xf, void *);
  int get_fwd_metric(IPAddress ip);
  int get_rev_metric(IPAddress ip);

  Vector <IPAddress> get_neighbors();
  void update_link(IPAddress from, IPAddress to, 
		   int fwd_small, int rev_small,
		   int fwd_1, int rev_1,
		   int fwd_2, int rev_2,
		   int fwd_5, int rev_5,
		   int fwd_11, int rev_11
		   ); 

  int get_delivery_rate(int rate, IPAddress from, IPAddress to);

  int get_tx_rate(EtherAddress);
  void get_rate_and_tput(int *tput, int *rate, 
			 int fwd_small,
			 int fwd_1,
			 int fwd_2, 
			 int fwd_5, 
			 int fwd_11,  
			 int rev_small,
			 int rev_1,
			 int rev_2,
			 int rev_5,
			 int rev_11);
  class LinkInfo {
  public:
    IPOrderedPair _p;
    int _fwd;
    int _rev;
    int _fwd_rate;
    int _rev_rate;

    int _fwd_small;
    int _fwd_1;
    int _fwd_2;
    int _fwd_5;
    int _fwd_11;


    int _rev_small;
    int _rev_1;
    int _rev_2;
    int _rev_5;
    int _rev_11;

    struct timeval _last;
    LinkInfo() { }
    LinkInfo(IPOrderedPair p) {
      _p = p;
    }
  };

  typedef HashMap<IPOrderedPair, LinkInfo> LTable;
  class LTable _links;
private:
  ETTStat *_ett_stat;
  LinkTable *_link_table;
  IPAddress _ip;

  int _weight_1;
  int _weight_2;
  int _weight_5;
  int _weight_11;
  bool _enable_twoway;
};

CLICK_ENDDECLS
#endif
