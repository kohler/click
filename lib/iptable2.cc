/*
 * iptable2.{cc,hh} -- a fast IP routing table. Implementation based on "Small
 * Forwarding Tables for Fast Routing Lookups" by Mikael Degermark, Andrej
 * Brodnik, Svante Carlsson and Stephen Pink.
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


#define MT_MAX         675
#define HACK  (MT_MAX + 5)     // arbitrary value > MT_MAX

bool IPTable2::_mt_done = false;                // built maptable?
u_int8_t IPTable2::_maptable[MT_MAX+1][8];      // See Degermark SIGCOMM '97
u_int16_t IPTable2::_mask2index[256][256];      // For mapping masks to _maptable

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
  int i;

  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid && (_v[i]._dst == dst) && (_v[i]._mask == mask)) {
      _v[i]._valid = 0;
      entries--;
      dirty = true;
      return;
    }
  }
}

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
  // just in time. Change this to timer.
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

  // Hackflag? offset is not offset but pointer to routing table.
  if(ten == HACK) {
    index = six;
    goto done;
  }

  u_int16_t pix = baseindex1[bix] + six + _maptable[ten][bit+1];
  index = l1ptrs[pix];

done:
  gw = _v[index & 0x3fff]._gw;
  return(true);
}




// sets bits in bitvector based on highest bits in IP address. the lowest bytes
// in the bitvector are affected before the larger ones.
void
IPTable2::set_all_bits(u_int16_t high16, int bit_index, u_int16_t masked, int router_entry, int value, Vector<int> &affected)
{
  int b;
  if(bit_index == 16) {
    b = set_single_bit(bitvector1, bit_index, 16, value);
    affected.push_back(b);
    l1ptrs.push_back(((masked & 0xffff0000) ? CHUNK : NEXT_HOP) | router_entry);
    return;
  }

  value = high16 >> (15-bit_index);

  // Depending on 0 or 1: set correct bit at level 16. Do this in a way that the
  // lowest bits get set first. In order.
  if(value & 0x0001) {
    b = set_single_bit(bitvector1, bit_index, 16, (value >> 1));
    l1ptrs.push_back(NEXT_HOP | router_entry);
    set_all_bits(high16, bit_index+1, masked, router_entry, value, affected);
  } else {
    set_all_bits(high16, bit_index+1, masked, router_entry, value, affected);
    b = set_single_bit(bitvector1, bit_index+1, 16, value | 0x0001);
    l1ptrs.push_back(NEXT_HOP | router_entry);
  }
  affected.push_back(b);
}

// Builds the whole Degermark structure
//
// sort routing table;
// for every IP address
//   1. set bits in bitvector from least sign. to most sign. while adding
//      pointers to l1ptrs.
//
//   2. Write the codewords that correspond to 0 or 1 bitvectors
//
// Build codewords array and baseindex array.
//
void
IPTable2::build()
{
  for(register int i = 0; i < 4096; i++)
    codewords1[i] = bitvector1[i] = 0;
  for(register int i = 0; i < 1024; i++)
    baseindex1[i] = 0;
  l1ptrs.clear();

  // Make sorted routing table and delete old one.
  struct Entry* sorted = new struct Entry[entries];
  int sorted_index = 0;
  for(register int i = 0; i < _v.size(); i++)
    if(_v[i]._valid)
      sorted[sorted_index++] = _v[i];
  sort(sorted, sorted_index);
  clear();

  Vector<int> affected;
  for(int i = 0; i < sorted_index; i++) {
    // Build new (simple) routing table based on sorted version.
    add(sorted[i]._dst, sorted[i]._mask, sorted[i]._gw);

    // masked, high16, dst, mask and 0x0000ffff in network order!
    // masked == (IP address range from router table)
    u_int32_t masked = (sorted[i]._dst & sorted[i]._mask);
    u_int16_t high16 = masked & 0x0000ffff;
    if(high16 == 0)
      continue;
    high16 = ntohs(high16);

    // set bits in bitvector for this routing table entry
    affected.clear();
    set_all_bits(high16, 0, masked, i, 0, affected);

    // For all affected double-bytes in bitvector, check whether or not they are
    // 0 or 1 and if so, administer the routertable entry in codewords for which
    // that happened. This is an optimization proposed by Degermark.
    u_int16_t bv;
    int af_index;
    for(int j = 0; j < affected.size(); j++) {
      af_index = affected[j];
      bv = bitvector1[af_index];
      if(bv == 0 || bv == 1) {
        codewords1[af_index] = ((HACK) << 6);
        codewords1[af_index] += i;
      }
    }
  }

  // Although we called add() in this loop, the routing table is not dirty.
  dirty = false;
  delete[] sorted;

  // build codewords1 array and base index array based on current bitvector.
  int bi1_index = 0;
  int bits_so_far = 0;
  u_int16_t bv;
  for(int j = 0; j < 4096; j++) {
    bv = bitvector1[j];

    // Write record-index of maptable in upper 10 bits. There is no such index
    // for records where bv == 0 or 1
    int mt_index = -1;
    if(bv & 0xfffe) { // if (bv != 0x0000 && bv != 0x0001)
      mt_index = mt_indexfind(bitvector1[j]);
      codewords1[j] = mt_index << 6;
    }

    // In most cases: lower 6 bits contain offset.
    //
    // For bv == 0 or 1, there might be hack-entry in the corresponding
    // codeword. Don't overwrite that. It points directly to the router table
    // and not to the maptable.
    if(j & 0x0003) { // not dividable by 4
      if(((codewords1[j] & 0xffc0) >> 6) != HACK) { // not hacked.
        codewords1[j] += bits_so_far;
      } else {
        // only codewords that correspond to a bitvector doublebyte that is 0 or
        // 1 can have their hackflag set.
        assert(bv == 0 || bv == 1);
      }
    } else if(j) {
      if(bi1_index)
        baseindex1[bi1_index] = baseindex1[bi1_index-1] + bits_so_far;
      else
        baseindex1[bi1_index] = bits_so_far;
      bi1_index++;
      bits_so_far = 0;
    }
    // else if(j == 0)
    //   codewords1[j].offset = 0; has already that value

    if(bv & 0xfffe) // if (bv != 0x0000 && bv != 0x0001)
      bits_so_far += _maptable[mt_index][7] & 0x0f;
    else
      bits_so_far += (bv & 0x0001); // no entry in maptable exists.
  }
}


// Sets a bit in bitvector and returns the index of the doublebyte in bitvector
// that it has changed.
inline int
IPTable2::set_single_bit(u_int16_t bitvector[],
                         u_int32_t from_level, // 1 .. 16
                         u_int32_t to_level,   // 1 .. 16
                         u_int32_t value)      // 0000 0000 .. ffff ffff
{
  assert(from_level <= to_level);

  // How many levels are there between?
  unsigned int leveldiff = to_level - from_level;
  unsigned bits_before = value << leveldiff;

  // write bit with index bits_before that is in byte
  int bvi = bits_before >> 4;

  // Bitnumber to write within byte.
  int bit_index = bits_before & 0x000f;

  // Set the bit.
  bitvector[bvi] |= (0x0001 << bit_index);
  return(bvi);
}




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
    // _maptable[i][0] = set_bits = 0;
    set_bits = 0;
    for(u_int8_t j = 0; j < 16; j++) {
      if(mask & 0x8000)
        set_bits++;
      mask <<= 1;

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
// masks to record numbers for maptable.
Vector<u_int16_t>
IPTable2::all_masks(int length)
{
  assert(length >= 0 && length <= 4);

  Vector<u_int16_t> v;
  if(length == 0) {
    v.push_back(0x0001);
    return v;
  }

  v = all_masks(length-1);

  // Create the shifted version of all of masks in v.
  Vector<u_int16_t> shifted_v;
  for(int i = 0; i < v.size(); i++)
    shifted_v.push_back(v[i] << (0x0001 << (length-1)));

  // On level 4, don't put 1 in there. Optimization.
  Vector<u_int16_t> v_new;
  if(length != 4)
    v_new.push_back(0x0001);

  // Create all masks of length 2^length.
  int mt_index = 0;
  u_int16_t mask;
  for(int i = 0; i < shifted_v.size(); i++) {
    for(int j = 0; j < v.size(); j++) {
      mask = shifted_v[i] | v[j];
      v_new.push_back(mask);
      if(length == 4)
        _mask2index[mask >> 8][mask & 0x00ff] = mt_index++;
    }
  }

  return v_new;
}

// generate Vector template instance
#include "vector.cc"
template class Vector<IPTable2::Entry>;

/*
 *
 * QUICKSORT. Taken from GNU glibc; slightly adjusted for speedup.
 *
 */

/* Byte-wise swap two items of size SIZE. */
#define SWAP(a, b, size)						      \
  do									      \
    {									      \
      register size_t __size = (size);					      \
      register char *__a = (a), *__b = (b);				      \
      do								      \
	{								      \
	  char __tmp = *__a;						      \
	  *__a++ = *__b;						      \
	  *__b++ = __tmp;						      \
	} while (--__size > 0);						      \
    } while (0)

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
#define MAX_THRESH 4

/* Stack node declarations used to store unfulfilled partition obligations. */
typedef struct
  {
    char *lo;
    char *hi;
  } stack_node;

/* The next 4 #defines implement a very fast in-line stack abstraction. */
#define STACK_SIZE	(8 * sizeof(unsigned long int))
#define PUSH(low, high)	((void) ((top->lo = (low)), (top->hi = (high)), ++top))
#define	POP(low, high)	((void) (--top, (low = top->lo), (high = top->hi)))
#define	STACK_NOT_EMPTY	(stack < top)


/* Order size using quicksort.  This implementation incorporates
   four optimizations discussed in Sedgewick:

   1. Non-recursive, using an explicit stack of pointer that store the
      next array partition to sort.  To save time, this maximum amount
      of space required to store an array of MAX_INT is allocated on the
      stack.  Assuming a 32-bit integer, this needs only 32 *
      sizeof(stack_node) == 136 bits.  Pretty cheap, actually.

   2. Chose the pivot element using a median-of-three decision tree.
      This reduces the probability of selecting a bad pivot value and
      eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
      insertion sort to order the MAX_THRESH items within each partition.
      This is a big win, since insertion sort is faster for small, mostly
      sorted array segments.

   4. The larger of the two sub-partitions is always pushed onto the
      stack first, with the algorithm then concentrating on the
      smaller partition.  This *guarantees* no more than log (n)
      stack size is needed (actually O(1) in this case)!  */

void
IPTable2::sort(void *const pbase, size_t total_elems)
{
  register char *base_ptr = (char *) pbase;
  size_t size = sizeof(struct Entry);

  /* Allocating SIZE bytes for a pivot buffer facilitates a better
     algorithm below since we can do comparisons directly on the pivot. */
  char *pivot_buffer = new char[size];
  const size_t max_thresh = MAX_THRESH * size;

  if (total_elems == 0)
    /* Avoid lossage with unsigned arithmetic below.  */
    return;

  if (total_elems > MAX_THRESH)
    {
      char *lo = base_ptr;
      char *hi = &lo[size * (total_elems - 1)];
      /* Largest size needed for 32-bit int!!! */
      stack_node stack[STACK_SIZE];
      stack_node *top = stack + 1;

      while (STACK_NOT_EMPTY)
        {
          char *left_ptr;
          char *right_ptr;

	  char *pivot = pivot_buffer;

	  /* Select median value from among LO, MID, and HI. Rearrange
	     LO and HI so the three values are sorted. This lowers the
	     probability of picking a pathological pivot value and
	     skips a comparison for both the LEFT_PTR and RIGHT_PTR. */

	  char *mid = lo + size * ((hi - lo) / size >> 1);

	  if (entry_compare((void *) mid, (void *) lo) < 0)
	    SWAP (mid, lo, size);
	  if (entry_compare((void *) hi, (void *) mid) < 0)
	    SWAP (mid, hi, size);
	  else
	    goto jump_over;
	  if (entry_compare((void *) mid, (void *) lo) < 0)
	    SWAP (mid, lo, size);
	jump_over:;
	  memcpy (pivot, mid, size);
	  pivot = pivot_buffer;

	  left_ptr  = lo + size;
	  right_ptr = hi - size;

	  /* Here's the famous ``collapse the walls'' section of quicksort.
	     Gotta like those tight inner loops!  They are the main reason
	     that this algorithm runs much faster than others. */
	  do
	    {
	      while (entry_compare((void *) left_ptr, (void *) pivot) < 0)
		left_ptr += size;

	      while (entry_compare((void *) pivot, (void *) right_ptr) < 0)
		right_ptr -= size;

	      if (left_ptr < right_ptr)
		{
		  SWAP (left_ptr, right_ptr, size);
		  left_ptr += size;
		  right_ptr -= size;
		}
	      else if (left_ptr == right_ptr)
		{
		  left_ptr += size;
		  right_ptr -= size;
		  break;
		}
	    }
	  while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((size_t) (right_ptr - lo) <= max_thresh)
            {
              if ((size_t) (hi - left_ptr) <= max_thresh)
		/* Ignore both small partitions. */
                POP (lo, hi);
              else
		/* Ignore small left partition. */
                lo = left_ptr;
            }
          else if ((size_t) (hi - left_ptr) <= max_thresh)
	    /* Ignore small right partition. */
            hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr))
            {
	      /* Push larger left partition indices. */
              PUSH (lo, right_ptr);
              lo = left_ptr;
            }
          else
            {
	      /* Push larger right partition indices. */
              PUSH (left_ptr, hi);
              hi = right_ptr;
            }
        }
    }

  /* Once the BASE_PTR array is partially sorted by quicksort the rest
     is completely sorted using insertion sort, since this is efficient
     for partitions below MAX_THRESH size. BASE_PTR points to the beginning
     of the array to sort, and END_PTR points at the very last element in
     the array (*not* one beyond it!). */

#define min(x, y) ((x) < (y) ? (x) : (y))

  {
    char *const end_ptr = &base_ptr[size * (total_elems - 1)];
    char *tmp_ptr = base_ptr;
    char *thresh = min(end_ptr, base_ptr + max_thresh);
    register char *run_ptr;

    /* Find smallest element in first threshold and place it at the
       array's beginning.  This is the smallest array element,
       and the operation speeds up insertion sort's inner loop. */

    for (run_ptr = tmp_ptr + size; run_ptr <= thresh; run_ptr += size)
      if (entry_compare((void *) run_ptr, (void *) tmp_ptr) < 0)
        tmp_ptr = run_ptr;

    if (tmp_ptr != base_ptr)
      SWAP (tmp_ptr, base_ptr, size);

    /* Insertion sort, running from left-hand-side up to right-hand-side.  */

    run_ptr = base_ptr + size;
    while ((run_ptr += size) <= end_ptr)
      {
	tmp_ptr = run_ptr - size;
	while (entry_compare((void *) run_ptr, (void *) tmp_ptr) < 0)
	  tmp_ptr -= size;

	tmp_ptr += size;
        if (tmp_ptr != run_ptr)
          {
            char *trav;

	    trav = run_ptr + size;
	    while (--trav >= run_ptr)
              {
                char c = *trav;
                char *hi, *lo;

                for (hi = lo = trav; (lo -= size) >= tmp_ptr; hi = lo)
                  *hi = *lo;
                *hi = c;
              }
          }
      }
  }
}


int
IPTable2::entry_compare(const void *e1, const void *e2)
{
  unsigned dst1 = ntohl(((struct IPTable2::Entry *)e1)->_dst);
  unsigned dst2 = ntohl(((struct IPTable2::Entry *)e2)->_dst);

  if(dst1 < dst2)
    return -1;
  if(dst1 > dst2)
    return 1;
  return 0;
}
