#ifndef GRIDGENERICRT_HH
#define GRIDGENERICRT_HH

// public interface class to Grid routetables.  yes, i know, this is
// not a great abstraction but it works for now.

#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include "grid.hh"

class GridGenericRouteTable : public Element {

public:
  
  GridGenericRouteTable() { }
  GridGenericRouteTable(int ninputs, int noutputs) : Element(ninputs, noutputs) { }

  struct RouteEntry {

    class IPAddress      dest_ip;      // IP address of this destination

    class grid_location  dest_loc;     // location of dest, as contained in its route ads
    bool                 loc_good;     // is location any good?
    unsigned short       loc_err;      // error in metres
    
    class EtherAddress   next_hop_eth; // hardware address of next hop
    class IPAddress      next_hop_ip;  // IP address of next hop
    
    unsigned int         seq_no;
    unsigned char        num_hops;

    RouteEntry(const IPAddress &dst, 
	       bool lg, unsigned short le, const grid_location &l, 
	       const EtherAddress &nhe, const IPAddress &nhi,
	       unsigned int sn, unsigned char nh) :
      dest_ip(dst), dest_loc(l), loc_good(lg), loc_err(le), 
      next_hop_eth(nhe), next_hop_ip(nhi),
      seq_no(sn), num_hops(nh) 
    { }

    RouteEntry() : loc_good(false), loc_err(0), seq_no(0), num_hops(0) { }
  };


  // return false if there is no entry for a GW, else fill in entry
  virtual bool current_gateway(RouteEntry &entry) = 0;

  // return false if there is no entry for this dest, else fill in entry
  virtual bool get_one_entry(IPAddress &dest_ip, RouteEntry &entry) = 0;

  // append all the current route entries to vec.  You should clear
  // vec before calling this method, if desired.
  virtual void get_all_entries(Vector<RouteEntry> &vec) = 0;
  
  virtual ~GridGenericRouteTable() { }
};



#endif
