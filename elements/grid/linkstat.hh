#ifndef LINKSTATHH
#define LINKSTATHH

/*
 * =c
 * LinkStat(AiroInfo, WINDOW)
 * =s Grid
 * =d
 *
 * Expects Ethernet packets as input.  Queries the AiroInfo element
 * for information about the packet sender's latest transmission stats
 * (quality, signal strength).  Also attempts to track broadcast loss
 * rate for the last WINDOW milliseconds.  These stats are then made
 * available to the PingPong element for sending back to the
 * transmitter's LinkTracker.
 *
 * =a
 * AiroInfo, LinkTracker, PingPong */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>

#include "airoinfo.hh"
CLICK_DECLS

class LinkStat : public Element {
private:
  AiroInfo *_ai;
  unsigned int _window; // msecs
  
  struct bcast_t {
    struct timeval when; // jiffies
    unsigned seq; // seq_no of broadcast
  };

  BigHashMap<EtherAddress, Vector<bcast_t> > _bcast_stats;

  static String read_stats(Element *, void *);
  static String read_bcast_stats(Element *, void *);
  
  static String read_window(Element *, void *);
  static int write_window(const String &, Element *, void *, ErrorHandler *);
  
  void add_bcast_stat(const EtherAddress &e, unsigned int seqno);

 public:
    struct stat_t {
    int qual;
    int sig;
    struct timeval when;
  };
  BigHashMap<EtherAddress, stat_t> _stats;

  /*
   * look up most recent stats for E.  LAST is the local rx time of
   * the last packet used to collect stats for E, NUM_RX is the number
   * actually received during the interval [LAST-WINDOW, LAST], WINDOW
   * is the interval size in milliseconds, and NUM_EXPECTED is the
   * number we think should have seen, based on observed sequence
   * numbers.  Returns true if we have some data, else false.  
   */
  bool get_bcast_stats(const EtherAddress &e, struct timeval &last, unsigned int &window,
		       unsigned int &num_rx, unsigned int &num_expected);

  void remove_all_stats(const EtherAddress &e);

  LinkStat();
  ~LinkStat();
  
  const char *class_name() const		{ return "LinkStat"; }
  const char *processing() const		{ return "a/a"; }
  
  LinkStat *clone() const;

  void add_handlers();

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
};

CLICK_ENDDECLS
#endif
