#ifndef CLICK_FRAGMENTRESENDER_HH
#define CLICK_FRAGMENTRESENDER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
#include <click/timer.hh>
#include <click/notifier.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

FragmentResender(mode, BSSID)

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
  wifi_cl [2] -> wifi_encap :: FragmentResender() -> ...

=a

EtherEncap */

class FragmentResender : public Element { public:
  
  FragmentResender();
  ~FragmentResender();

  const char *class_name() const	{ return "FragmentResender"; }
  const char *processing() const	{ return "lh/l"; }
  
  int initialize (ErrorHandler *);
  void run_timer ();
  void push(int port, Packet *);
  Packet *pull(int);

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Vector<int> get_packets();
  Packet * do_resend();
  Packet * ack_request();
  void process_ack(Packet *p);

  void add_handlers();


  void print_window();
  struct PacketInfo {
    EtherAddress dst;
    struct timeval last_tx;
    Vector <int> frag_status; /* 1 if acked, 0 otherwize */
    Vector <int> frag_sends; /* 1 if acked, 0 otherwize */
    Packet *p;
    PacketInfo() { }    
    PacketInfo(const PacketInfo &p) 
      : dst(p.dst), 
	last_tx(p.last_tx),
	p(p.p)
    {
      for (int x = 0; x < p.frag_status.size(); x++) {
	frag_status.push_back(p.frag_status[x]);
	frag_sends.push_back(p.frag_sends[x]);
      }
    }
    bool done() {
      for (int y =0; y < frag_status.size(); y++) {
	if (!frag_status[y]) {
	  return false;
	}
      }
      return true;
    }
  };


  typedef HashMap<int, PacketInfo> PacketInfoTable;
  typedef PacketInfoTable::const_iterator PIIter;

  
  class PacketInfoTable _packets;
  bool _debug;
  unsigned _et;

  bool _wait_for_ack;
  bool _send_ack;

  unsigned _window_size;
  unsigned _ack_timeout_ms;
  Timer _timer;


  Vector<struct fragid> outstanding;
  int resend_index;
  int resend_limit;
  int _max_retries;

  void fix();
 private:

};

CLICK_ENDDECLS
#endif
