#ifndef IPTABLE_HH
#define IPTABLE_HH

// IP routing table.
// Lookup by longest prefix.
// Each entry contains a gateway and an output index.

#include <click/glue.hh>
#include <click/vector.hh>

class IPTable {
public:
  IPTable();
  ~IPTable();

  bool lookup(unsigned dst, unsigned &gw, int &index) const;
  void add(unsigned dst, unsigned mask, unsigned gw, int index);
  void del(unsigned dst, unsigned mask);
  void clear() { _v.clear(); }

private:
  struct Entry {
    unsigned _dst;
    unsigned _mask;
    unsigned _gw;
    int _index;
    int _valid;
  };
  Vector<Entry> _v;
};

#endif
