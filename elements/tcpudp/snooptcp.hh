#ifndef CLICK_SNOOPTCP_HH
#define CLICK_SNOOPTCP_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/ipflowid.hh>
#include <click/hashmap.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
CLICK_DECLS

/*
 * SnoopTCP
 * Input 0 should be data packets from a server to a mobile host.
 *   Those packets are sent out output 0.
 * Input 1 should ACK packets from the mobile host back to the server.
 *   Those packets are sent out output 1.
 * Output 2 sends retransmitted packets destined for the mobile host.
 * Output 3 sends generated packets destined for the server.
 *
 * Expects IP packets (no ether header).
 *
 * XXX - Retransmit timers not implemented
 */

#define DEBUG

class SnoopTCP : public Element { public:

  SnoopTCP() CLICK_COLD;
  ~SnoopTCP() CLICK_COLD;

  const char *class_name() const		{ return "SnoopTCP"; }
  const char *port_count() const		{ return "2/4"; }
  const char *processing() const		{ return "aa/aahh"; }
  const char *flow_code() const			{ return "xyz/xy"; }

  int initialize(ErrorHandler *) CLICK_COLD;

  void push(int port, Packet *);
  Packet *pull(int port);

 private:

  Packet *handle_packet(int, Packet *);

  struct SCacheEntry {
    Packet *packet;
    unsigned seq;
    unsigned size;
    Timestamp snd_time;
    int num_rxmit;
    int sender_rxmit;

    void clear();
  };

  struct PCB;

  HashMap<IPFlowID, PCB *> _map;

  PCB *find(unsigned s_ip, unsigned short s_port,
	    unsigned mh_ip, unsigned short mh_port, bool);

};


struct SnoopTCP::PCB {

  enum { S_CACHE_SIZE = 1024,
	 S_CACHE_HIGHWATER = (9*S_CACHE_SIZE / 10) };

  SCacheEntry _s_cache[S_CACHE_SIZE];
  int _head;
  int _tail;

  unsigned _s_una;		// first unacked SEQ #
  unsigned _s_max;		// first as-yet-unsent data SEQ #
  unsigned _mh_last_ack;	// last ACK received from MH
  unsigned short _mh_last_win;	// advertised window size

  int _mh_expected_dup_acks;
  int _mh_dup_acks;
  int _dup_acks;

  bool _s_exists : 1;
  bool _s_alive : 1;
  bool _mh_exists : 1;
  bool _mh_alive : 1;

  int next_i(int i) const	{ return (i+1) % S_CACHE_SIZE; }
  int prev_i(int i) const	{ return (i ? i-1 : S_CACHE_SIZE-1); }

  PCB() CLICK_COLD;
  ~PCB() CLICK_COLD;

  void clear(bool is_s);
  void initialize(bool is_s, const click_tcp *, int datalen) CLICK_COLD;

  int s_cache_size() const	{ return (_head >= _tail ? _head - _tail : S_CACHE_SIZE - (_tail - _head)); }

  void clean(unsigned);

  Packet *s_data(Packet *, const click_tcp *, int datalen);
  void s_ack(Packet *, const click_tcp *, int datalen);

  void mh_data(Packet *, const click_tcp *, int datalen);

  void mh_new_ack(unsigned ack);
  Packet *mh_dup_ack(Packet *, const click_tcp *, unsigned ack);
  Packet *mh_ack(Packet *, const click_tcp *, int datalen);

};

CLICK_ENDDECLS
#endif
