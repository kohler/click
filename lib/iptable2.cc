/*
 * iptable2.{cc,hh} -- a fast IP routing table. Implementation based on "Small
 * Forwarding Tables for Fast Routing Lookups" by Mikael Degermark, Andrej
 * Brodnik, Svante Carlsson and Stephen Pink. Sigcomm '97. Also called the
 * "Lulea Algorithm". Ocassionaly I refer to sections from this Article,
 * Thomer M. Gil
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "iptable2.hh"
#include "integers.hh"


#define MT_MAX                   675
#define DIRECT_POINTER  (MT_MAX + 5)            // arbitrary value > MT_MAX

bool IPTable2::_mt_done = false;
u_int8_t IPTable2::_maptable[MT_MAX+1][8];
u_int16_t IPTable2::_mask2index[256][256];      // see build_maptable()

IPTable2::IPTable2()
  : entries(0), dirty(false)
{
  if(!_mt_done)
    build_maptable();
}

IPTable2::~IPTable2()
{
}


// Adds an entry to the simple routing table if not in there already.
// Allows only one gateway for equals dst/mask combination.
void
IPTable2::add(unsigned dst, unsigned mask, unsigned gw)
{
  for(int i = 0; i < _v.size(); i++)
    if(_v[i]._valid && (_v[i]._dst == dst) && (_v[i]._mask == mask))
      return;

  struct Entry e;
  e._dst = dst;
  e._mask = mask;
  e._gw = gw;
  e._valid = 1;
  _v.push_back(e);

  entries++;
  dirty = true;
}


// Deletes an entry from the routing table.
void
IPTable2::del(unsigned dst, unsigned mask)
{
  for(int i = 0; i < _v.size(); i++){
    if(_v[i]._valid && (_v[i]._dst == dst) && (_v[i]._mask == mask)) {
      _v[i]._valid = 0;
      entries--;
      dirty = true;
      return;
    }
  }
}

// Returns the i-th record.
bool
IPTable2::get(int i, unsigned &dst, unsigned &mask, unsigned &gw)
{
  assert(i >= 0 && i < _v.size());

  if(i < 0 || i >= _v.size() || _v[i]._valid == 0) {
    dst = mask = gw = 0;
    return(false);
  }

  dst = _v[i]._dst;
  mask = _v[i]._mask;
  gw = _v[i]._gw;
  return(true);
}


// Use the fast routing table to perform the lookup.
bool
IPTable2::lookup(unsigned dst, unsigned &gw, int &index)
{
#if 0
  // Just in time. XXX: Change this to timer.
  if(dirty) {
    build();
    dirty = false;
  }
    
  dst = ntohl(dst);

  u_int16_t ix = (dst & 0xfff00000) >> 20;      // upper 12 bits.
  u_int16_t bix = (dst & 0xffc00000) >> 22;     // upper 10 bits.
  u_int8_t bit = (dst & 0x000f0000) >> 16;      // lower  4 of upper 16 bits.
  u_int16_t codeword = codewords1[ix];
  u_int16_t ten = (codeword & 0xffc0) >> 6;     // upper 10 bits.
  u_int8_t six = codeword & 0x003f;             // lower  6 bits.

  // Offset is not offset but pointer to routing table. See 4.2.1 of Degermark.
  if(ten == DIRECT_POINTER) {
    index = six;
    goto done;
  }

  // Figure 10 in Degermark is wrong.
  int offset = _maptable[ten][bit >> 1];
  if(bit & 0x0001) // odd
    offset &= 0x0f;
  else
    offset >>= 4;
  u_int16_t pix = baseindex1[bix] + six + offset;
  index = l1ptrs[pix];

done:
  gw = _v[index & 0x3fff]._gw;
  return(true);

#endif
  // just in time. Change this to timer.
  if(dirty) {
    build();
    dirty = false;
  }
    
  dst = ntohl(dst);

  click_chatter("dst = %x", dst);
  u_int16_t ix = (dst & 0xfff00000) >> 20;      // upper 12 bits.
  click_chatter("ix: %x", ix);
  u_int16_t bix = (dst & 0xffc00000) >> 22;     // upper 10 bits.
  click_chatter("bix: %x", bix);
  u_int8_t bit = (dst & 0x000f0000) >> 16;      // lower  4 of upper 16 bits.
  click_chatter("bit: %x", bit);
  u_int16_t codeword = codewords1[ix];
  click_chatter("codeword = %x", codeword);
  u_int16_t ten = (codeword & 0xffc0) >> 6;     // upper 10 bits.
  click_chatter("ten = %x", ten);
  click_chatter("maptable for record %x\n-----------", ten);
  for(int i=0; i<7; i++) {
    click_chatter("%x", _maptable[ten][i]);
  }
  u_int8_t six = codeword & 0x003f;             // lower  6 bits.
  click_chatter("six = %x", six);

  // Hackflag? offset is not offset but pointer to routing table.
  if(ten == DIRECT_POINTER) {
    index = six;
    goto done;
  }

  click_chatter("baseindex1[bix] = (d) %d", baseindex1[bix]);
  click_chatter("_maptable[ten][bit >> 1] = (x) %x", _maptable[ten][bit >> 1]);
  int maptable_offset = _maptable[ten][bit >> 1];
  if(bit & 0x0001) // odd
    maptable_offset &= 0x0f;
  else
    maptable_offset >>= 4;
  click_chatter("maptable_offset = (x) %x", maptable_offset);

  u_int16_t pix = baseindex1[bix] + six + maptable_offset;
  click_chatter("pix = (d) %d", pix);
  index = l1ptrs[pix];
  click_chatter("index = (x) %x", index);

  for(int i = 0; i < l1ptrs.size(); i++)
    click_chatter("l1prts[(d) %d] = (x) %x", i, l1ptrs[i]);


done:
  gw = _v[index & 0x3fff]._gw;
  return(true);
}

// Builds the whole structure as described by the Lulea Algorithm.
// After execution l1ptrs, codewords1 and baseindex1 represent routing table.
//
// (NOT FINISHED: level 2 and 3)
//
// bitvector1 contains bitvector as described in section 4.2 of Degermark.
// bit_admin contains an entry for each bit in bitvector1.
// Both are temporary.
void
IPTable2::build()
{
  u_int16_t bitvector1[4096];
  struct bit bit_admin[65536];

  for(register int i = 0; i < 65536; i++)
    bit_admin[i].from_level = bit_admin[i].value = 0;
  for(register int i = 0; i < 4096; i++)
    codewords1[i] = bitvector1[i] = 0;
  for(register int i = 0; i < 1024; i++)
    baseindex1[i] = 0;
  l1ptrs.clear();

  Vector<int> affected;
  for(int i = 0; i < entries; i++) {
    if(_v[i]._valid == 0)
      continue;

    // masked, high16, dst, mask and 0x0000ffff in network order!
    // masked == (IP address range from router table)
    u_int32_t masked = (_v[i]._dst & _v[i]._mask);
    u_int16_t high16 = masked & 0x0000ffff;
    if(high16 == 0)
      continue;
    high16 = ntohs(high16);

    click_chatter("Inserting %x", high16);

    // set bits in bitvector for this routing table entry
    affected.clear();
    set_all_bits(bitvector1, bit_admin, high16, i, affected);

    // For all affected shorts in bitvector, check whether or not they are
    // 0 or 1 and if so apply the optimization described in section 4.2.1 of
    // Degermark.
    u_int16_t bv;
    int af_index;
    for(int j = 0; j < affected.size(); j++) {
      af_index = affected[j];
      bv = bitvector1[af_index];
      if(!(bv & 0xfffe)) { // bv == 0 || bv == 1
        codewords1[af_index] = ((DIRECT_POINTER) << 6);
        codewords1[af_index] += i;
      }
    }
  }

  // Now build l1ptrs, based on set bits in bitvector1. See section 4.2 of
  // Degermark.
  for(register int i = 0; i < 65536; i++)
    if(bit_admin[i].value) {
      l1ptrs.push_back(bit_admin[i].value);
      click_chatter("Pushed bit %d (== %x) on vector (index = %d) : %x", i, bit_admin[i].value, l1ptrs.size()-1, l1ptrs[l1ptrs.size()-1]);
    }


  // First entry of baseindex1 always 0.
  int bi1_idx = 0;
  baseindex1[bi1_idx++] = 0;

  int mt_index = 0, bits_so_far = 0;
  for(int j = 0; j < 4096; j++) {
    u_int16_t bv = bitvector1[j];
    click_chatter("bitvector1[%d] = %x", j, bitvector1[j]);

    // Write record-index of maptable in upper 10 bits. No such index exists
    // for records where bv == 0 or bv == 1 (see section 4.2.1).
    if(bv & 0xfffe) { // if (bv != 0x0000 && bv != 0x0001)
      mt_index = mt_indexfind(bitvector1[j]);
      codewords1[j] = mt_index << 6;
    }

    // Lower 6 bits of codewords contain offset. Every fourth codeword starts
    // with offset 0.
    //
    // For codewords related to a bv == 0 or bv == 1, there might be direct
    // routing table index in the related codeword (section 4.2.1). Don't
    // overwrite that entry.

    // Every non 4th codeword.
    if(j & 0x0003) {
      if(((codewords1[j] & 0xffc0) >> 6) != DIRECT_POINTER) {
        codewords1[j] += bits_so_far;
      }
      // else {
      //   this means that related bv == 0 or bv == 1
      // }


    // Every 4th codeword: 0 and set baseindex.
    } else if(j) {
      baseindex1[bi1_idx] = baseindex1[bi1_idx-1] + bits_so_far;
      click_chatter("baseindex1[%d] = %d", bi1_idx, baseindex1[bi1_idx]);
      bi1_idx++;
      bits_so_far = 0;
    }

    // Raise bits_so_far.

    // The number of bits in a short from the bitvector can be retrieved from
    // maptable, although we still have to check for the most sign. bit since
    // maptable only tells the # of set bits BEFORE the x-th bit.
    //
    // For bv == 0 or bv == 1 there is no maptable entry. Just check.
    if(bv & 0xfffe) // bv != 0x0000 && bv != 0x0001
      bits_so_far += ((_maptable[mt_index][7] & 0x0f) + ((bv & 0x8000) ? 1 : 0));
    else
      bits_so_far += (bv & 0x0001);

    click_chatter("codewords1[%d] is %x", j, codewords1[j]);
  }
}




// Sets all necessary bits in bitvector based on a (part of a) IP address. Read
// section 4.2 of Degermark to see which bits are set and what they mean.
//
// bitvector    - bitvector as described in 4.2 of Degermark
// bit_admin    - table to temp. store info on each bit in bitvector
// high16       - 16 most sign. bits of IP address
// rtable_idx   - index in sorted routing table where high16 came from
// affected     - vector with indices of altered shorts in bitvector 
void
IPTable2::set_all_bits(u_int16_t bitvector[],
                       struct bit bit_admin[],
                       u_int16_t high16,
                       int rtable_idx,
                       Vector<int> &affected)
{
  u_int16_t value;

  u_int16_t headinfo = (NEXT_HOP | rtable_idx);
  for(int i = 0; i < 16; i++) {
    value = high16 >> (15-i);

    // Every node must have 0 or 2 children. Every node has to set a 1 in the
    // bitvector where its range starts. See Degermark figure 5.
    if(value & 0x0001)
      set_single_bit(bitvector, bit_admin, i, 16, (value >> 1), headinfo, affected);
    else
      set_single_bit(bitvector, bit_admin, i+1, 16, value | 0x0001, headinfo, affected);
  }

  click_chatter("Setting bit on level 16");
  u_int16_t masked = _v[rtable_idx]._dst & _v[rtable_idx]._mask;
  headinfo = (((masked & 0xffff0000) ? CHUNK : NEXT_HOP) | rtable_idx);
  set_single_bit(bitvector, bit_admin, 16, 16, value, headinfo, affected);
  return;
}



// Sets a single bit in the supplied bitvector.
//
// See section 4. What Degermark doesn't tell you is that expanding the prefix
// tree to be complete causes annoying collisions of bits in the bitvector:
// different entries of the routing table might try to set the same bit in the
// bitvector. A node closest to the bitvector can override bits set by nodes
// further away from the bitvector. Figure 4 illustrates this (the two rightmost
// nodes; e2 'hides' entries of e1. e2 can do this because it is closer to the
// bitvector).
//
// bitvector    - where bit has to be set
// bit_admin    - contains info on what entry caused which bit to be set.
// from_level   - since prefix tree is complete, every node at whatever level
// can cause a bit to be set down in the bitvector. From_level tells this method
// from which level this bit is set. Later needed to see who can override other
// ones bits.
//
// to_level     - 16 for level 1, 24 for level 2, 32 for level 3.
// value        - The prefix of an IP address. The number of relevant bits
// equals from_level.
//
// headinfo     - The headinfo to be placed in l1ptrs in a later phase.
inline void
IPTable2::set_single_bit(u_int16_t bitvector[],
                         struct bit bit_admin[],
                         u_int32_t from_level,
                         u_int32_t to_level,
                         u_int32_t value,
                         u_int16_t headinfo,
                         Vector<int> &affected)
{
  assert(from_level <= to_level);

  unsigned int leveldiff = to_level - from_level;
  unsigned bit_in_vector = value << leveldiff;
  int vector_index = bit_in_vector >> 4;
  int bit_in_short = bit_in_vector & 0x000f;

  // Only override set bit if it is set from a lower level in prefix tree.
  if(bitvector[vector_index] & (0x0001 << bit_in_short))
    if(bit_admin[bit_in_vector].from_level >= from_level)
      return;
  bitvector[vector_index] |= (0x0001 << bit_in_short);
  bit_admin[bit_in_vector].value = headinfo;
  bit_admin[bit_in_vector].from_level = from_level;
  affected.push_back(vector_index);

  click_chatter("bitvector[%d] is %x", vector_index, bitvector[vector_index]);
}




// Returns the index in _maptable where mask can be found.
inline u_int16_t
IPTable2::mt_indexfind(u_int16_t mask)
{
  assert(mask != 0x0000 && mask != 0x0001);
  return _mask2index[mask >> 8][mask & 0x00ff];
}



// Builds maptable as described by Degermark.
void
IPTable2::build_maptable()
{
  u_int8_t set_bits;
  u_int16_t mask;

  Vector<u_int16_t> masks = all_masks(4);
  assert(masks.size() == MT_MAX + 1);

  for(int i = 0; i < masks.size(); i++) {
    mask = masks[i];
    _maptable[i][0] = set_bits = 0;
    for(u_int8_t j = 1; j < 16; j++) {
      if(mask & 0x0001)
        set_bits++;
      mask >>= 1;

      // j even: set upper 4 bits.
      if((j & 0x0001) == 0)
        _maptable[i][j >> 1] = (set_bits << 4);
      else
        _maptable[i][j >> 1] |= set_bits;
    }
  }

  _mt_done = true;
}

// Generates all possible masks of length 2^length AND a mapping from these
// See section 4.2 formula (2).
//
// _mask2index is a two-dimensional array that translates masks to the index in
// maptable related to that mask.
Vector<u_int16_t>
IPTable2::all_masks(int length, bool toplevel = true)
{
  assert(length >= 0 && length <= 4);

  Vector<u_int16_t> v;
  if(length == 0) {
    v.push_back(0x0001);
    return v;
  }

  v = all_masks(length-1, false);

  // Create the shifted version of all of masks in v.
  Vector<u_int16_t> shifted_v;
  for(int i = 0; i < v.size(); i++)
    shifted_v.push_back(v[i] << (0x0001 << (length-1)));

  // On toplevel, don't put 1 in there. See section 4.2.1
  Vector<u_int16_t> v_new;
  if(!toplevel)
    v_new.push_back(0x0001);

  // Create all masks of length 2^length.
  int mt_index = 0;
  for(int i = 0; i < shifted_v.size(); i++) {
    for(int j = 0; j < v.size(); j++) {
      u_int16_t mask = shifted_v[i] | v[j];
      v_new.push_back(mask);
      if(toplevel) {
         click_chatter("Record %d is for mask %x", mt_index, mask);
        _mask2index[mask >> 8][mask & 0x00ff] = mt_index++;
      }
    }
  }

  return v_new;
}

// generate Vector template instance
#include "vector.cc"
template class Vector<IPTable2::Entry>;
