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

  // is fast routing table up-to-date?
  bool dirty;

  // maptable
  static bool _mt_done;
  static u_int8_t _maptable[][];
  static u_int16_t _mask2index[][];


#define NEXT_HOP   0x8000
#define CHUNK      0xc000

  // data structures for level 1
  u_int16_t bitvector1[4096];
  u_int16_t codewords1[4096];
  u_int16_t baseindex1[1024];
  Vector<u_int16_t> l1ptrs;

  // Used by public add.
  void add(unsigned dst, unsigned mask, unsigned gw, bool update);

  // maptable
  Vector<u_int16_t> IPTable2::all_masks(int);
  void build_maptable();
  inline u_int16_t mt_indexfind(u_int16_t);

  // Used for building level 1 data structure
  void build();
  inline int set_single_bit(u_int16_t*, u_int32_t, u_int32_t, u_int32_t);
  void set_all_bits(u_int16_t high16, int bit_index, u_int16_t masked, int router_entry, int value, Vector<int> &affected);

  static void sort(void *const pbase, size_t total_elems);
  static int entry_compare(const void *e1, const void *e1);
};

#endif
