#ifndef IP6TABLE_HH
#define IP6TABLE_HH

// IP6 routing table.
// Lookup by longest prefix.
// Each entry contains a gateway and an output index.

#include "glue.hh"
#include "vector.hh"
#include "ip6address.hh"

class IP6Table {
public:
  IP6Table();
  ~IP6Table();

  bool lookup(IP6Address dst, IP6Address &gw, int &index);
  void add(IP6Address dst, IP6Address mask, IP6Address gw, int index);
  void del(IP6Address dst, IP6Address mask);
  void clear() { _v.clear(); }

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

#endif
