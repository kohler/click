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
class SrcrStat;

class ETTMetric : public LinkMetric {
  
public:

  ETTMetric();
  ~ETTMetric();

  const char *class_name() const { return "ETTMetric"; }
  const char *processing() const { return AGNOSTIC; }
  ETTMetric *clone()  const { return new ETTMetric; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void add_handlers();

  void *cast(const char *);

  int get_fwd_metric(IPAddress ip);
  int get_rev_metric(IPAddress ip);


  void update_link(SrcrStat *ss, IPAddress from, IPAddress to, int fwd, int rev);

  class LinkInfo {
  public:
    IPOrderedPair _p;
    int _small_fwd;
    int _small_rev;
    int _big_fwd;
    int _big_rev;
    struct timeval _last_small;
    struct timeval _last_big;
    LinkInfo() { }
    LinkInfo(IPOrderedPair p) {
      _p = p;
    }

    void update_small(int fwd, int rev) {
      _small_fwd = fwd;
      _small_rev = rev;
    }
    void update_big(int fwd, int rev) {
      _big_fwd = fwd;
      _big_rev = rev;
    }
    
    int fwd_metric() {
      if (_big_fwd > 0 && _small_rev > 0) {
	return (100 * 100 * 100) / (_big_fwd * _small_rev);
      }
      
      return 7777;
    }
    int rev_metric() {
      if (_big_rev > 0 && _small_fwd > 0) {
	return (100 * 100 * 100) / (_big_rev * _small_fwd);
      }
      return 7777;

    }
  };

  typedef HashMap<IPOrderedPair, LinkInfo> LTable;
  class LTable _links;
private:
  SrcrStat *_ss_small;
  SrcrStat *_ss_big;
  LinkTable *_link_table;
};

CLICK_ENDDECLS
#endif
