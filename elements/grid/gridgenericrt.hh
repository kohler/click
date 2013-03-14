#ifndef GRIDGENERICRT_HH
#define GRIDGENERICRT_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include "grid.hh"
CLICK_DECLS

// public interface class to Grid routetables.  yes, i know, this is
// not a great abstraction but it works for now.

class GridGenericRouteTable : public Element {

public:

  GridGenericRouteTable() { }

  struct RouteEntry {

    class IPAddress      dest_ip;      // IP address of this destination

    struct grid_location dest_loc;     // location of dest, as contained in its route ads
    bool                 loc_good;     // is location any good?
    unsigned short       loc_err;      // error in metres

    class EtherAddress   next_hop_eth; // hardware address of next hop
    class IPAddress      next_hop_ip;  // IP address of next hop
    unsigned char        next_hop_interface; // interface of next hop

  protected:
    unsigned int         _seq_no;
    unsigned char        _num_hops;
  public:
    unsigned int         seq_no()   const { return _seq_no;   }
    unsigned char        num_hops() const { return _num_hops; }
    bool                 good()     const { return _num_hops != 0; }
    bool                 broken()   const { return !good(); }

    RouteEntry(const IPAddress &dst,
	       bool lg, unsigned short le, const grid_location &l,
	       const EtherAddress &nhe, const IPAddress &nhi, unsigned char interface,
	       unsigned int sn, unsigned char nh) :
      dest_ip(dst), dest_loc(l), loc_good(lg), loc_err(le),
      next_hop_eth(nhe), next_hop_ip(nhi), next_hop_interface(interface),
      _seq_no(sn), _num_hops(nh)
    { }

    RouteEntry() : loc_good(false), loc_err(0), next_hop_interface(0), _seq_no(0), _num_hops(0) { }
  };


  // return false if there is no entry for a GW, else fill in entry with best choice for gateway
  virtual bool current_gateway(RouteEntry &entry) = 0;

  // return false if there is no entry for this dest, else fill in entry
  virtual bool get_one_entry(const IPAddress &dest_ip, RouteEntry &entry) = 0;

  // append all the current route entries to vec.  You should clear
  // vec before calling this method, if desired.
  virtual void get_all_entries(Vector<RouteEntry> &vec) = 0;

  // return the number of neighbors we can hear from directly.  This
  // may be larger than (but never less than) the number of nodes with
  // 1-hop route, because some neighbors aren't their own best next
  // hop.
  virtual unsigned get_number_direct_neigbors() {
    Vector<RouteEntry> v;
    get_all_entries(v);

    // assume all direct neighbors are one-hop neighbors
    int num_nbrs = 0;
    for (int i = 0; i < v.size(); i++)
      if (v[i].num_hops() == 1 && v[i].good())
	num_nbrs++;
    return num_nbrs;
  }

  virtual ~GridGenericRouteTable() { }
};

CLICK_ENDDECLS
#endif
