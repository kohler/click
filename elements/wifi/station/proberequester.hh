#ifndef CLICK_PROBEREQUESTER_HH
#define CLICK_PROBEREQUESTER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

class ProbeRequester : public Element { public:
  
  ProbeRequester();
  ~ProbeRequester();

  const char *class_name() const	{ return "ProbeRequester"; }
  const char *processing() const	{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }
  void add_handlers();

  void send_probe_request();


  bool _debug;
  EtherAddress _eth;
  String _ssid;
  class AvailableRates *_rtable;

 private:


};

CLICK_ENDDECLS
#endif
