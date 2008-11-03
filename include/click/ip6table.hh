// -*- c-basic-offset: 2; related-file-name: "../../lib/ip6table.cc" -*-
#ifndef CLICK_IP6TABLE_HH
#define CLICK_IP6TABLE_HH
#include <click/glue.hh>
#include <click/vector.hh>
#include <click/ip6address.hh>
CLICK_DECLS

// IP6 routing table.
// Lookup by longest prefix.
// Each entry contains a gateway and an output index.

class IP6Table { public:

  IP6Table();
  ~IP6Table();

  bool lookup(const IP6Address &dst, IP6Address &gw, int &index) const;

  void add(const IP6Address &dst, const IP6Address &mask, const IP6Address &gw, int index);
  void del(const IP6Address &dst, const IP6Address &mask);
  void clear()				{ _v.clear(); }
  String dump();

 private:

  struct Entry {
    IP6Address _dst;
    IP6Address _mask;
    IP6Address _gw;
    int _index;
    int _valid;
  };
  Vector<Entry> _v;

};

CLICK_ENDDECLS
#endif
