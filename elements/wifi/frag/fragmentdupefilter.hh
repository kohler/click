#ifndef CLICK_FRAGMENTDUPEFILTER_HH
#define CLICK_FRAGMENTDUPEFILTER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
#include <click/timer.hh>
#include <click/dequeue.hh>
#include "frag.hh"
CLICK_DECLS

/*
=c

FragmentDupeFilter(mode, BSSID)

=s encapsulation, Wifi -> Ethernet

Turns 80211 pdupefilterets into ethernet pdupefilterets encapsulates pdupefilterets in Ethernet header

=d

Mode is one of:
0x00 STA->STA
0x01 STA->AP
0x02 AP->STA
0x03 AP->AP

 BSSID is an ethernet address


=e


  wifi_cl :: Classifier (0/00%0c, 
                         0/04%0c,
                         0/08%0c);

  wifi_cl [0] -> Discard; //mgt 
  wifi_cl [1] -> Discard; //ctl
  wifi_cl [2] -> wifi_encap :: FragmentDupeFilter() -> ...

=a

EtherEncap */

class FragmentDupeFilter : public Element { public:
  
  FragmentDupeFilter();
  ~FragmentDupeFilter();

  const char *class_name() const	{ return "FragmentDupeFilter"; }
  const char *processing() const		{ return "a/ah"; }
  
  void notify_noutputs(int);

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  void send_dupefilter(EtherAddress src);

  Packet * simple_action(Packet *p);

  struct DstInfo {
    EtherAddress src;
    DEQueue<struct fragid> frags;
    struct timeval last;
    DstInfo() { }
    DstInfo(EtherAddress s) { src = s; }
    
  };

  

  typedef HashMap<EtherAddress, DstInfo> FragTable;
  typedef FragTable::const_iterator FIter;

  class FragTable _frags;

  EtherAddress _en;
  unsigned _et;
  void add_handlers();
  unsigned _window_size;


  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif
