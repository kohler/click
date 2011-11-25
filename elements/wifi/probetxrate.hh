#ifndef CLICK_PROBETXRATE_HH
#define CLICK_PROBETXRATE_HH
#include <click/element.hh>
#include <click/deque.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <elements/wifi/bitrate.hh>

CLICK_DECLS
class AvailableRates;

/*
=c

ProbeTXRate([I<KEYWORDS>])

=s Wifi

Madwifi wireless bit-rate selection algorithm

=d

Probematically determine the txrate for a give ethernet dst

=over 8

=item RATE_WINDOW
How long to remember tx packets

=item STEPUP
a value from 0 to 100 of what the percentage must be before
the rate is increased

=item STEPDOWN

a value from 0 to 100. when the percentage of successful packets
falls below this value, the card will try the next lowest rate

=item BEFORE_SWITCH

how many packets must be received before you calculate the rate
for a give host. i.e. if you set this to 4, each rate will try
at least 4 packets before switching up or down.

This element should be used in conjunction with SetTXRate
and a wifi-enabled kernel module. (like hostap or airo).

=back 8

=a SetTXRate, WifiTXFeedback
*/


class ProbeTXRate : public Element { public:

  ProbeTXRate();
  ~ProbeTXRate();

  const char *class_name() const		{ return "ProbeTXRate"; }
  const char *port_count() const		{ return "2/0-2"; }
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
    Timestamp _when;
    int _rate;
    bool _success;
    int _time;
    int _tries;

    tx_result(const Timestamp &t, int rate,
	      bool success, int time, int tries) :
      _when(t),
      _rate(rate),
      _success(success),
      _time(time),
      _tries(tries)
    { }
    tx_result() {}
  };


  struct DstInfo {
  public:

    EtherAddress _eth;
    Deque<tx_result> _results;

    Vector<int> _rates;

    Vector<int> _packets;
    Vector<int> _total_time;
    Vector<int> _total_success;
    Vector<int> _total_fail;
    Vector<int> _perfect_time;
    Vector<int> _total_tries;


    unsigned _count;
    DstInfo() {
    }

    DstInfo(EtherAddress eth, Vector<int> rates) {
      _eth = eth;
      _rates = rates;
      _packets = Vector<int>(_rates.size(), 0);
      _total_time = Vector<int>(_rates.size(), 0);
      _total_success = Vector<int>(_rates.size(), 0);
      _total_fail = Vector<int>(_rates.size(), 0);
      _total_tries = Vector<int>(_rates.size(), 0);
      _perfect_time = Vector<int>(_rates.size(), 0);


      _count = 0;
      for (int x = 0; x < _rates.size(); x++) {
	_perfect_time[x] = calc_usecs_wifi_packet(1500, _rates[x], 0);
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

    void check () {
      for (int x = 0; x < _rates.size(); x++) {
	int sum_success = 0;
	int sum_fail = 0;
	int sum_time = 0;
	for (int y = 0; y < _results.size(); y++) {
	  if (_results[y]._rate != _rates[x]) {
	    continue;
	  }
	  if (_results[y]._success) {
	    sum_success++;
	  } else {
	    sum_fail++;
	  }
	  sum_time += _results[y]._time;
	}

      if (sum_success != _total_success[x]) {
	click_chatter("rate %d mismatch success %d %d\n",
		      _rates[x],
		      sum_success, _total_success[x]);
      }
      if (sum_fail != _total_fail[x]) {
	click_chatter("rate %d mismatch fail %d %d\n",
		      _rates[x],
		      sum_fail, _total_fail[x]);
      }
      if (sum_time != _total_time[x]) {
	click_chatter("rate %d mismatch time %d %d\n",
		      _rates[x],
		      sum_time, _total_time[x]);
      }
      }
    }

    void add_result(const Timestamp &now, int rate, int tries, int success,
		    int time) {
      int ndx = rate_index(rate);
      if (!rate || ndx < 0 || ndx > _rates.size()){
	return;
      }
      _total_time[ndx] += time;
      _total_tries[ndx] += tries;
      _packets[ndx]++;
      if (success) {
	_total_success[ndx]++;
      } else {
	_total_fail[ndx]++;
      }
      _results.push_back(tx_result(now, rate,
				   success, time, tries));
    }



    void trim(const Timestamp &ts) {
      while (_results.size() && _results[0]._when < ts) {
	tx_result t = _results[0];
	_results.pop_front();

	int ndx = rate_index(t._rate);

	if (ndx < 0 || ndx >= _rates.size()) {
	  click_chatter("%s: ???",
			__func__);
	  continue;
	}
	if (t._success) {
	  _total_success[ndx]--;
	} else {
	  _total_fail[ndx]--;
	}
	_packets[ndx]--;
	_total_time[ndx] -= t._time;
	_total_tries[ndx] -= t._tries;

      }
    }

    int average(int ndx) {
      if (!_total_success[ndx]) {
	return 0;
      }

      return _total_time[ndx] / _total_success[ndx];
    }

    int best_rate_ndx() {
      int best_ndx = -1;
      int best_time = 0;
      bool found = false;
      if (!_rates.size()) {
	return -1;
      }
      for (int x = 0; x < _rates.size(); x++) {
	if (_total_success[x]) {
	  int time = _total_time[x] / _total_success[x];
	  if (!found || time < best_time) {
	    best_ndx = x;
	    best_time = time;
	    found = true;
	  }
	}
      }
      if (found) {
	return best_ndx;
      }

      for (int x = _rates.size()-1; x > 0; x--) {
	if (_total_fail[x] < 3) {
	  return x;
	}
      }
      return 0;
    }

    Vector<int> pick_rate() {
      int best_ndx = best_rate_ndx();

      if (_rates.size() == 0) {
	click_chatter("no rates to pick from for %s\n",
		      _eth.unparse().c_str());
	return _rates;
      }

      if (best_ndx < 0) {
	/* no rate has had a successful packet yet */
	return _rates;
      }

      int best_time = average(best_ndx);
      Vector<int> possible_rates;
      for (int x = 0; x < _rates.size(); x++) {
	if (best_time < _perfect_time[x]) {
	  /* couldn't possibly be better */
	  continue;
	}

	if (_total_fail[x] > 3 && !_total_success[x]) {
	  continue;
	}
	possible_rates.push_back(_rates[x]);
      }

      return possible_rates;

    }


  };
  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  NeighborTable _neighbors;
  EtherAddress _bcast;
  int _rate_window_ms;
  Timestamp _rate_window;

  AvailableRates *_rtable;


  bool _active;
  unsigned _original_retries;
  unsigned _min_sample;
};

CLICK_ENDDECLS
#endif
