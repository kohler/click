#ifndef CLICK_DUPEFILTER_HH
#define CLICK_DUPEFILTER_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/dequeue.hh>
#include <click/hashmap.hh>
CLICK_DECLS

/*
 * =c
 * DupeFilter([TAG] [, KEYWORDS])
 * =s debugging
 * =d
 * Assumes input packets are SR packets (ie a sr_pkt struct from 
 * sr.hh). Prints out a description of those packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =a
 * Print, SR
 */

class DupeFilter : public Element {
  
 public:
  
  DupeFilter();
  ~DupeFilter();
  
  const char *class_name() const		{ return "DupeFilter"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

  static String static_read_stats(Element *xf, void *);
  static String static_read_debug(Element *xf, void *);
  static int static_write_debug(const String &arg, Element *e,
				void *, ErrorHandler *errh);
  void add_handlers();

  class PathInfo {
  public:
    Path _p;
    struct timeval _last;
    int _dupes;
    int _packets;
    DEQueue<int> _sequences; //most recently received seq nos
    PathInfo(Path p) {
      _p = p;
    }
    PathInfo () { }
    void clear() {
      _dupes = 0;
      _packets = 0;
      _sequences.clear();
      click_gettimeofday(&_last);
    }
  };

  typedef HashMap <Path, PathInfo> PathTable;
  typedef PathTable::const_iterator PathIter;

  PathTable _paths;
  int _window;
  int _debug;
};

CLICK_ENDDECLS
#endif
