#ifndef CLICK_PROBETXRATE_HH
#define CLICK_PROBETXRATE_HH
#include <click/element.hh>
#include <click/dequeue.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
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

static inline int rate_to_tput(int rate) {
  return 100 * rate;
  switch (rate) {
  case 108:
    return 0;
  case 96:
    return 0;
  case 72:
    return 0;
  case 48:
    return 0;
  case 36:
    return 0;
  case 24:
    return 0;
  case 22:
    return 0;
  case 18:
    return 0;
  case 12:
    return 0;
  case 11:
    return 0;
  case 4:
    return 0;
  case 2:
    return 0;
    
  default:
    return 0;
  }
}


class ProbeTXRate : public Element { public:
  
  ProbeTXRate();
  ~ProbeTXRate();
  
  const char *class_name() const		{ return "ProbeTXRate"; }
  const char *processing() const		{ return "ah/a"; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }


  void push (int, Packet *);

  Packet *pull(int);

  void process_feedback(Packet *);
  Packet *assign_rate(Packet *);
  void add_handlers();



  EtherAddress _eth;
  bool _debug;
  unsigned _packet_size_threshold;
  String print_rates();
 private:
  struct tx_result {
    struct timeval _when;
    int _rate;
    int _tries;
    bool _success;
    
    tx_result(const struct timeval &t, int rate, int tries, bool success) : 
      _when(t), 
      _rate(rate), 
      _tries(tries), 
      _success(success)
    { }
    tx_result() {}
  };

  
  struct DstInfo {
  public:

    EtherAddress _eth;
    DEQueue<tx_result> _results;

    Vector<int> _rates;
    
    
    Vector<int> _total_tries;
    Vector<int> _total_success;

    DstInfo() { 
    }

    DstInfo(EtherAddress eth) { 
      _eth = eth;
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
	}
	_total_tries[ndx] -= t._tries;
	
      }
    }


    int pick_rate() {
      int best_ndx = -1;
      int best_tput = 0;
      for (int x = 0; x < _rates.size(); x++) {
	if (_total_success[x]) {
	  int tput = rate_to_tput(_rates[x]) * _total_tries[x] / _total_success[x];
	  if (tput > best_tput) {
	    best_ndx = x;
	    best_tput = tput;
	  }
	}
      }
      
      if (random() % 9 == 0) {
	int r = random() % _rates.size();
	//click_chatter("picking random %d\n", r);
	return _rates[r];
      }
      if (best_ndx < 0) {
	int r = random() % _rates.size();
	click_chatter("no rates to pick from..random %d\n", r);
	return _rates[r];
      }

      return _rates[best_ndx];
    }


    void add_result(struct timeval now, int rate, int tries, int success) {
      int ndx = rate_index(rate);
      if (ndx < 0 || ndx > _rates.size()){
	return;
      }
      _total_tries[ndx] += tries;
      if (success) {
	_total_success[ndx]++;
      }
      _results.push_back(tx_result(now, rate, tries, success));
    }
  };
  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  class NeighborTable _neighbors;
  EtherAddress _bcast;
  int _rate_window_ms;
  struct timeval _rate_window;

  class AvailableRates *_rtable;

};

CLICK_ENDDECLS
#endif
