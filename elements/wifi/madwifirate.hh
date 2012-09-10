#ifndef CLICK_MADWIFIRATE_HH
#define CLICK_MADWIFIRATE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include <click/timer.hh>
#include <elements/wifi/availablerates.hh>
CLICK_DECLS

/*
=c

MadwifiRate([I<KEYWORDS>])

=s Wifi

Madwifi wireless bit-rate selection algorithm

=d
Rate Control present in the Madwifi driver
(http://sourceforge.net/project/madwifi).

=a
SetTXRate, FilterTX, AutoRateFallback
*/


class MadwifiRate : public Element { public:

  MadwifiRate() CLICK_COLD;
  ~MadwifiRate() CLICK_COLD;

  const char *class_name() const		{ return "MadwifiRate"; }
  const char *port_count() const		{ return "2/0-2"; }
  const char *processing() const		{ return "ah/a"; }
  const char *flow_code() const			{ return "#/#"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const		{ return true; }
  int initialize(ErrorHandler *) CLICK_COLD;
  void run_timer(Timer *);
  void adjust_all();
  void adjust(EtherAddress);


  void push (int, Packet *);

  Packet *pull(int);

  void add_handlers() CLICK_COLD;
  static String static_print_stats(Element *e, void *);
  String print_rates();


  EtherAddress _bcast;
  void assign_rate(Packet *);

  void process_feedback(Packet *);

  int _stepup;
  int _stepdown;
  bool _debug;
  unsigned _offset;
  Timer _timer;

  unsigned _packet_size_threshold;

  struct DstInfo {
  public:

    EtherAddress _eth;
    Vector<int> _rates;

    int _credits;

    int _current_index;

    int _successes;
    int _failures;
    int _retries;

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

    int pick_rate() {
      if (_rates.size() == 0) {
	click_chatter("no rates to pick from for %s\n",
		      _eth.unparse().c_str());
	return 2;
      }
      if (_current_index > 0 && _current_index < _rates.size()) {
	return _rates[_current_index];
      }
      return _rates[0];
    }

    int pick_alt_rate() {
      if (_rates.size() == 0) {
	click_chatter("no rates to pick from for %s\n",
		      _eth.unparse().c_str());
	return 2;
      }
      return _rates[0];
    }
  };

  typedef HashMap<EtherAddress, DstInfo> NeighborTable;
  typedef NeighborTable::const_iterator NIter;

  NeighborTable _neighbors;

  AvailableRates *_rtable;
  bool _alt_rate;
  bool _active;
  int _period;
};

CLICK_ENDDECLS
#endif
