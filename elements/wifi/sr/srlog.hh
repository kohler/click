#ifndef CLICK_SRLOG_HH
#define CLICK_SRLOG_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/dequeue.hh>
#include <clicknet/wifi.h>

CLICK_DECLS

/*
 * =c
 * SRLog(I<KEYWORKDS>)
 * 
 * =s Wifi
 * Log transmit feedback stats for later analysis.
 * 
 * =d 
 * SRLog records the size, timestamp, and other infor for
 * each packet that passed through.  The list of
 * recorded data can be dumped (and cleared) by repeated calls to the
 * read handler 'log'.
 *
 * =h log read-only
 * Print as much of the list of logged packets as possible, clearing 
 * printed packets from the log.
 * =h more read-only
 * Returns how many entries are left to be read.
 * =h reset write-only
 * Clears the log.
 *
 */

class SRLog : public Element { public:
  
  SRLog();
  ~SRLog();
  
  const char *class_name() const		{ return "SRLog"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flow_code() const			{ return "#/#"; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();

  class Fo {
  public:
    char header[500];
    int rate;
    int retries;
    u_int32_t flags;
    
  };
  
  DEQueue<Fo> _p;
  DEQueue<struct timeval> _t;

  bool _active;
};

CLICK_ENDDECLS
#endif
