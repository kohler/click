#ifndef CLICK_GRID_PROXY_HH
#define CLICK_GRID_PROXY_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <click/hashmap.hh>
#include <clicknet/ip.h>
CLICK_DECLS

/*
 * =c
 * GridProxy(IP)
 * =s Grid
 * =d
 * a simple mobile-ip proxy
 * Input 0: ipip packets from a gateway
 * Input 1: ip packets for a mobile host
 * Output 0: ip packets for the outside world
 * Output 1: ipip packets for current "gateway" machine
 *
 * GridProxy tracks the last gateway a host sent an ip packet
 * through and when it receives packets for that host, it sends
 * then to the gateway it was last heard through using an
 * ipip tunnel.
 */
class GridProxy : public Element {

  class DstInfo {
  public:
    IPAddress _ip;
    Timestamp _last_updated;
    IPAddress _gw;
    DstInfo() {_ip = IPAddress(0); _gw = IPAddress(0); }
    DstInfo(IPAddress ip, IPAddress gw, Timestamp now) {
      _ip = ip;
      _last_updated = now;
      _gw = gw;
    }

  };
  typedef HashMap<IPAddress, DstInfo> ProxyMap;
  ProxyMap _map;

  click_ip _iph;
  uatomic32_t _id;
#if HAVE_FAST_CHECKSUM && FAST_CHECKSUM_ALIGNED
  bool _aligned;
#endif

  void reverse_mapping(Packet *p_in);
  void forward_mapping(Packet *p_in);


public:
    GridProxy() CLICK_COLD;
    ~GridProxy() CLICK_COLD;

    const char *class_name() const { return "GridProxy"; }
    const char *port_count() const { return "2/2"; }
    const char *processing() const { return PUSH; }
    const char *flags() const	   { return "A"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void add_handlers() CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    void push(int, Packet *);

  static String static_print_stats(Element *e, void *);
  String print_stats();


};

CLICK_ENDDECLS
#endif
