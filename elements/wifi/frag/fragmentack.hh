#ifndef CLICK_FRAGMENTACK_HH
#define CLICK_FRAGMENTACK_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
#include <click/timer.hh>
#include "frag.hh"
CLICK_DECLS

/*
=c

FragmentAck(mode, BSSID)

=s encapsulation, Wifi -> Ethernet

Turns 80211 packets into ethernet packets encapsulates packets in Ethernet header

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
  wifi_cl [2] -> wifi_encap :: FragmentAck() -> ...

=a

EtherEncap */

class FragmentAck : public Element { public:
  
  FragmentAck();
  ~FragmentAck();

  const char *class_name() const	{ return "FragmentAck"; }
  const char *processing() const		{ return "a/ah"; }
  
  void notify_noutputs(int);

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  void send_ack(EtherAddress src);

  Packet * simple_action(Packet *p);
  struct WindowInfo {
    EtherAddress src;
    Vector<struct fragid> frags_rx;
    struct timeval first_rx;
    bool waiting;
    WindowInfo() { }
    WindowInfo(EtherAddress s) { src = s; }
    
    bool add(struct fragid f) {
      for (int x = 0; x < frags_rx.size(); x++) {
	if (frags_rx[x] == f) {
	  return false;
	}
      }

      frags_rx.push_back(f);
      return true;
    }
    
  };

  

  typedef HashMap<EtherAddress, WindowInfo> FragTable;
  typedef FragTable::const_iterator FIter;

  class FragTable _frags;

  EtherAddress _en;
  unsigned _et;
  void add_handlers();
  unsigned _window_size;

  unsigned _ack_timeout_ms;

  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif
