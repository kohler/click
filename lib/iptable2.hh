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


  struct bit {
    u_int8_t  from_level;
    u_int16_t value;
  };

  // is fast routing table up-to-date?
  bool dirty;

  // maptable
  static bool _mt_done;
  static u_int8_t _maptable[][];
  static u_int16_t _mask2index[][];


#define NEXT_HOP   0x8000
#define CHUNK      0xc000

  // data structures for level 1
  u_int16_t codewords1[4096];
  u_int16_t baseindex1[1024];
  Vector<u_int16_t> l1ptrs;

  // Used by public add.
  void add(unsigned dst, unsigned mask, unsigned gw, bool update);

  // maptable
  Vector<u_int16_t> IPTable2::all_masks(int, bool toplevel = true);
  void build_maptable();
  inline u_int16_t mt_indexfind(u_int16_t);

  // Used for building level 1 data structure
  void build();
  void set_single_bit(u_int16_t bitvector[], struct bit bit_admin[],
      u_int32_t from_level, u_int32_t to_level, u_int32_t value,
      u_int16_t headinfo, Vector<int> &affected);
  void set_all_bits(u_int16_t bitvector[], struct bit bit_admin[], u_int16_t high16,
      int router_entry, Vector<int> &affected);
};

#endif
