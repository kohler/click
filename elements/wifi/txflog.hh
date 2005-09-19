#ifndef CLICK_TXFLOG_HH
#define CLICK_TXFLOG_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/dequeue.hh>
#include <clicknet/wifi.h>

CLICK_DECLS

/*
 * =c
 * TXFLog(I<KEYWORKDS>)
 * 
 * =s Wifi
 * Log transmit feedback stats for later analysis.
 * 
 * =d 
 * TXFLog records the size, timestamp, and other infor for
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

class TXFLog : public Element { public:
  
  TXFLog();
  ~TXFLog();
  
  const char *class_name() const		{ return "TXFLog"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flow_code() const			{ return "#/#"; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  Packet *simple_action(Packet *);

  void add_handlers();

  
  DEQueue<struct click_wifi_extra> _p;
  DEQueue<Timestamp> _t;

};

CLICK_ENDDECLS
#endif
