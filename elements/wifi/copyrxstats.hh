#ifndef CLICK_COPYRXSTATS_HH
#define CLICK_COPYRXSTATS_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * 
 * CopyRXStats(OFFSET)
 * 
 * =s wifi
 * 
 * Copies rate, signal and noise annotations into the packet at the
 * specified offset.  This is a cheap hack to make it simple to run
 * experiments and accumulate these stats per-packet.
 *
 * =over 8
 *
 */

class CopyRXStats : public Element { public:
  
  CopyRXStats();
  ~CopyRXStats();
  
  const char *class_name() const		{ return "CopyRXStats"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

 private:
  
  unsigned int _offset;
};

CLICK_ENDDECLS
#endif
