#ifndef CLICK_PROBETXRATE_HH
#define CLICK_PROBETXRATE_HH
#include <click/element.hh>
#include <click/dequeue.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
CLICK_DECLS

/*
 * =c
 * 
 * ProbeTXRate([I<KEYWORDS>])
 * 
 * =s wifi
 * 
 * Probematically determine the txrate for a give ethernet dst
 * =over 8
 *
 * =item RATE_WINDOW
 * 
 * How long to remember tx packets
 *
 * =item STEPUP
 *
 * a value from 0 to 100 of what the percentage must be before
 * the rate is increased
 * 
 * =item STEPDOWN
 *
 * a value from 0 to 100. when the percentage of successful packets
 * falls below this value, the card will try the next lowest rate
 *
 * =item BEFORE_SWITCH
 *
 * how many packets must be received before you calculate the rate
 * for a give host. i.e. if you set this to 4, each rate will try
 * at least 4 packets before switching up or down.
 *
 *
 *
 * This element should be used in conjunction with SetTXRate
 * and a wifi-enabled kernel module. (like hostap or airo).
 *
 * =a
 * SetTXRate, WifiTXFeedback
 */


class ProbeTXRate : public Element { public:
  
  ProbeTXRate();
  ~ProbeTXRate();
  
  const char *class_name() const		{ return "ProbeTXRate"; }
  const char *processing() const		{ return "ah/a"; }
  const char *flow_code() const			{ return "#/#"; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }


  void push (int, Packet *);

  Packet *pull(int);

  void process_feedback(Packet *);
  void assign_rate(Packet *);
  void add_handlers();



  bool _debug;
  unsigned _offset;
  unsigned _packet_size_threshold;
  String print_rates();

  struct tx_result {
    struct timeval _when;
    int _rate;
    int _retries;
    bool _success;
    
    tx_result(const struct timeval &t, int rate, int retries, bool success) : 
      _when(t), 
      _rate(rate), 
      _retries(retries), 
      _success(success)
    { }
    tx_result() {}
  };

  
  struct DstInfo {
  public:

    EtherAddress _eth;
    DEQueue<tx_result> _results;

    Vector<int> _rates;
    
    
    Vector<int> _total_usecs;
    Vector<int> _total_tries;
    Vector<int> _total_success;
    Vector<int> _total_fail;
    Vector<int> _perfect_usecs;

    DstInfo() { 
    }

    DstInfo(EtherAddress eth, Vector<int> rates) { 
      _eth = eth;
      _rates = rates;
      _total_usecs = Vector<int>(_rates.size(), 0);
      _total_tries = Vector<int>(_rates.size(), 0);
      _total_success = Vector<int>(_rates.size(), 0);
      _total_fail = Vector<int>(_rates.size(), 0);
      _perfect_usecs = Vector<int>(_rates.size(), 0);
      
      for (int x = 0; x < _rates.size(); x++) {
	_perfect_usecs[x] = calc_usecs_wifi_packet(1500, _rates[x], 0);
      }
    }

    int rate_index(int rate) {
      int ndx = 0;
      for (int x = 0; x < _rates.size(); x++) {
	if (rate == _rates[x]) {
	  ndx = x;
	  break;
	}
      }
      return (ndx == _rates.size()) ? -1 : ndx;
    }

    void trim(struct timeval t) {
      while (_results.size() && timercmp(&_results[0]._when, &t, <)) {
	tx_result t = _results[0];
	_results.pop_front();

	int ndx = rate_index(t._rate);

	if (ndx < 0 || ndx > _rates.size()) {
	  click_chatter("%s: ???", 
			__func__);
	  continue;
	}
	if (t._success) {
	  _total_success[ndx]--;
	} else {
	  _total_fail[ndx]--;
	}
	int usecs = calc_usecs_wifi_packet(1500, t._rate, t._retries);
	_total_usecs[ndx] -= usecs;
	_total_tries[ndx] -= (t._retries + 1);
	
      }
    }
    int best_rate_ndx() {
      int best_ndx = -1;
      int best_usecs = 0;
      bool found = false;
      if (!_rates.size()) {
	return -1;
      }
      for (int x = 0; x < _rates.size(); x++) {
	if (_total_success[x]) {
	  int usecs = _total_usecs[x] / _total_success[x];
	  if (!found || usecs < best_usecs) {
	    best_ndx = x;
	    best_usecs = usecs;
	    found = true;
	  }
	}
      }
      return (found) ? best_ndx : -1;
    }

    int pick_alt_rate() {
      /*
       * pick the fastest rate that hasn't failed yet.
       */
      int best_ndx = -1;
      int best_usecs = 0;
      bool found = false;
      if (!_rates.size()) {
	return -1;
      }
      for (int x = 0; x < _rates.size(); x++) {
	if (_total_success[x] && !_total_fail[x]) {
	  int usecs = _total_usecs[x] / _total_success[x];
	  if (!found || usecs < best_usecs) {
	    best_ndx = x;
	    best_usecs = usecs;
	    found = true;
	  }
	}
      }
      return (found) ? _rates[best_ndx] : _rates[0];
    }

    int pick_rate() {
      int best_ndx = best_rate_ndx();

      if (_rates.size() == 0) {
	click_chatter("no rates to pick from for %s\n", 
		      _eth.s().cc());
	return 2;
      }
      
      if (best_ndx < 0) {
	for (int x = _rates.size() - 1; x >= 0; x--) {
	  /* pick the first rate that hasn't failed yet */
	  if (_total_tries[x] == 0) {
	    return _rates[x];
	  }
	}
	/* no rate has had a successful packet yet. 
	 * pick the lowest rate possible */
	return _rates[0];
      }

      int best_usecs = _total_usecs[best_ndx] / _total_success[best_ndx];

      int probe_ndx = -1;
      for (int x = 0; x < _rates.size(); x++) {
	if (best_usecs < _perfect_usecs[x]) {
	  /* couldn't possibly be better */
	  continue;
	}
	

	if (_total_tries[x] && _total_tries[x] >=  2 * _total_success[x]) {
	  /* this rate doesn't work */
	  if (_rates[x] >= 22) {
	    /* give up now */
	    break;
	  }
	  continue;
	}
	
	if (_total_tries[x] > 10) {
	  continue;
	}
	
	probe_ndx = x;
	break;
      }
      
      if (probe_ndx == -1) {
	return _rates[best_ndx];
      }
      return _rates[probe_ndx];
    }


    void add_result(struct timeval now, int rate, int retries, int success) {
      int ndx = rate_index(rate);
      if (ndx < 0 || ndx > _rates.size()){
	return;
      }
      assert(rate);
      assert(retries >= 0);
      _total_usecs[ndx] += calc_usecs_wifi_packet(1500, rate, retries);
      _total_tries[ndx] += retries + 1;
      if (success) {
	_total_success[ndx]++;
      } else {
	_total_fail[ndx]++;
      }
      _results.push_back(tx_result(now, rate, retries, success));
    }
  };
  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  class NeighborTable _neighbors;
  EtherAddress _bcast;
  int _rate_window_ms;
  struct timeval _rate_window;

  class AvailableRates *_rtable;
  bool _filter_low_rates;
  bool _filter_never_success;
  bool _aggressive_alt_rate;

};

CLICK_ENDDECLS
#endif
