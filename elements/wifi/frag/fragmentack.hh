#ifndef CLICK_FRAGMENTACK_HH
#define CLICK_FRAGMENTACK_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
#include <click/timer.hh>
#include "frag.hh"
CLICK_DECLS

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

  FragTable _frags;

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
