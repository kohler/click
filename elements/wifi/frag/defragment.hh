#ifndef CLICK_DEFRAGMENT_HH
#define CLICK_DEFRAGMENT_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
CLICK_DECLS

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
    PacketInfo(EtherAddress s, int p, 
	       int fs, int nf) {
      src = s;
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
