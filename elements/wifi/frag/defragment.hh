#ifndef CLICK_DEFRAGMENT_HH
#define CLICK_DEFRAGMENT_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
CLICK_DECLS

/*
=c

Defragment(mode, BSSID)

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
  wifi_cl [2] -> wifi_encap :: Defragment() -> ...

=a

EtherEncap */

class Defragment : public Element { public:
  
  Defragment();
  ~Defragment();

  const char *class_name() const	{ return "Defragment"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();


  struct PacketInfo {

    EtherAddress src;
    int packet;
    int num_frags;
    int frag_size;
    int fragments_rx;

    Vector <Packet *> fragments;
    PacketInfo() {

    }
    PacketInfo(EtherAddress src, int p, 
	       int fs, int nf) {

      packet = p;
      frag_size = fs;
      num_frags = nf;
      fragments_rx = 0;

      fragments.clear();
      for (int x = 0; x < num_frags; x++) {
	fragments.push_back(0);
      }
    }
  };


  typedef HashMap<int, PacketInfo> PacketInfoTable;
  typedef PacketInfoTable::const_iterator PIIter;

  class PacketInfoTable _packets;
  bool _debug;
 private:

};

CLICK_ENDDECLS
#endif
