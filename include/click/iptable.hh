// -*- c-basic-offset: 2; related-file-name: "../../lib/iptable.cc" -*-
#ifndef CLICK_IPTABLE_HH
#define CLICK_IPTABLE_HH
#include <click/glue.hh>
#include <click/vector.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

// IP routing table.
// Lookup by longest prefix.
// Each entry contains a gateway and an output index.

class IPTable { public:

  IPTable();
  ~IPTable();

  bool lookup(IPAddress dst, IPAddress &gw, int &index) const;

  void add(IPAddress dst, IPAddress mask, IPAddress gw, int index);
  void del(IPAddress dst, IPAddress mask);
  void clear()				{ _v.clear(); }

 private:

  struct Entry {
    IPAddress dst;
    IPAddress mask;
    IPAddress gw;
    int index;
    bool valid() const			{ return mask || !dst; }
  };
  Vector<Entry> _v;

};

CLICK_ENDDECLS
#endif
