#ifndef LINKSTATHH
#define LINKSTATHH

/*
 * =c
 * LinkStat(AiroInfo)
 * =s Grid
 * =d
 *
 * Expects Ethernet packets as input.  Queries the AiroInfo element
 * for information about the packet sender's transmission stats
 * (quality, signal strength).
 *
 * =a
 * AiroInfo, LinkTracker, PingPong
 */

#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/glue.hh>
#include <click/etheraddress.hh>

#include "airoinfo.hh"

class LinkStat : public Element {

  AiroInfo *_ai;
  
  static String read_stats(Element *, void *);

 public:
  
  struct stat_t {
    int qual;
    int sig;
    struct timeval when;
  };

  BigHashMap<EtherAddress, stat_t> _stats;
  
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

#endif
