#ifndef IPTABLE2_HH
#define IPTABLE2_HH

// IP routing table.
// Lookup by longest prefix.
// Each entry contains a gateway and an output index.

#include "glue.hh"
#include "vector.hh"

class IPTable2 {
public:
  IPTable2();
  ~IPTable2();

  bool search(unsigned dst, unsigned mask, unsigned gw, int &index);
  bool exists(unsigned dst, unsigned mask, unsigned gw);
  bool get(int i, unsigned &dst, unsigned &mask, unsigned &gw);

  void add(unsigned dst, unsigned mask, unsigned gw);
  void del(unsigned dst, unsigned mask);
  int size() const;
  void clear() { _v.clear(); }

private:
  struct Entry {
    unsigned _dst;
    unsigned _mask;
    unsigned _gw;
    int _valid;
  };
  Vector<Entry> _v;
  int entries;

  struct mt_record {
    unsigned char lower : 4;
    unsigned char higher : 4;
  };

  struct mt_record _maptable[676][8];      // 676: see Degermark
  void build_maptable();
};

#endif
