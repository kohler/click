#ifndef IPTABLE2_HH
#define IPTABLE2_HH

// IP routing table.
// Lookup by longest prefix.
// Each entry contains a gateway and an output index.

#include "radix.hh"
#include "glue.hh"
#include "vector.hh"

class IPTable2 {
public:
  IPTable2();
  ~IPTable2();

  bool lookup(unsigned dst, unsigned &gw, int &index);
  void add(unsigned dst, unsigned mask, unsigned gw);
  void del(unsigned dst, unsigned mask);
  bool get(int i, unsigned &dst, unsigned &mask, unsigned &gw);
  void clear() { _v.clear(); entries = 0; }
  int size() const { return entries; }



private:

  // Simple routing table
  struct Entry {
    unsigned _dst;
    unsigned _mask;
    unsigned _gw;
    bool _valid;
  };
  Vector<Entry> _v;
  int entries;

  Radix *radix;

  // is fast routing table up-to-date?
  bool dirty;
};

#endif
