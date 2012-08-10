// -*- c-basic-offset: 4; related-file-name: "../../lib/bitvector.cc" -*-
#ifndef CLICK_BITVECTOR_HH
#define CLICK_BITVECTOR_HH
#include <click/glue.hh>
CLICK_DECLS

/** @file <click/bitvector.hh>
 * @brief Click's bitvector class. */

/** @class Bitvector
  @brief Vector of bits.

  The Bitvector class implements a vector of individually addressable bits.
  It supports bitwise operations such as |= and &= as well as the usual
  assignment and indexing operations.

  Bitvectors are stored as arrays of data words with type word_type, each
  containing wbits bits. For some purposes it may be faster or easier to
  manipulate data words directly. */
class Bitvector { public:

    class Bit;
    typedef bool (Bitvector::*unspecified_bool_type)() const;

    typedef uint32_t word_type;
    enum { wbits = 32, wshift = 5, wmask = 31 };

    /** @brief Construct an empty bitvector. */
    Bitvector()
	: _max(-1), _data(&_f0), _f0(0), _f1(0) {
    }

    /** @brief Construct an all-false bitvector with @a n elements.
     * @pre @a n >= 0 */
    explicit Bitvector(int n);

    /** @brief Construct a @a bit-valued length-1 bitvector. */
    explicit Bitvector(bool bit)
	: _max(0), _data(&_f0), _f0(bit), _f1(0) {
    }

    /** @brief Construct a @a bit-valued length-@a n bitvector.
     * @pre @a n >= 0 */
    Bitvector(int n, bool bit);

    /** @brief Construct a bitvector as a copy of @a x. */
    Bitvector(const Bitvector &x);

    /** @brief Destroy a bitvector.
     *
     * All outstanding Bit objects become invalid. */
    ~Bitvector() {
	if (_data != &_f0)
	    delete[] _data;
    }


    /** @brief Return the number of bits in the bitvector. */
    int size() const		{ return _max + 1; }

    /** @brief Return true iff the bitvector's bits are all false. */
    bool zero() const;

    /** @brief Return true iff the bitvector's bits are all false.
     * @sa zero() */
    operator unspecified_bool_type() const {
	return !zero() ? &Bitvector::zero : 0;
    }


    /** @brief Return the bit at position @a i.
     * @pre 0 <= @a i < size() */
    Bit operator[](int i);

    /** @overload */
    bool operator[](int i) const;

    /** @brief Return the bit at position @a i, extending if necessary.
     *
     * If @a i >= size(), then the bitvector is resize()d to length @a i+1,
     * which adds false bits to fill out the vector.
     *
     * @pre 0 <= @a i
     * @post @a i < size() */
    Bit force_bit(int i);


    inline int word_size() const;
    inline int max_word() const;
    inline word_type *words();
    inline const word_type *words() const;


    /** @brief Set all bits to false. */
    void clear();

    /** @brief Resize the bitvector to @a n bits.
     * @pre @a n >= 0
     *
     * Any bits added to the bitvector are false. */
    void resize(int n);

    /** @brief Set the bitvector to @a bit-valued length-@a n.
     * @pre @a n >= 0
     * @return *this */
    Bitvector &assign(int n, bool bit);

    /** @brief Set the bitvector to a copy of @a x.
     * @return *this */
    Bitvector &operator=(const Bitvector &x);


    /** @brief Check bitvectors for equality. */
    bool operator==(const Bitvector &x) const {
	return _max == x._max
	    && memcmp(_data, x._data, word_size() * sizeof(word_type)) == 0;
    }

    /** @brief Check bitvectors for inequality. */
    bool operator!=(const Bitvector &x) const {
	return !(*this == x);
    }


    /** @brief Return the bitwise negation of this bitvector. */
    inline Bitvector operator~() const;

    /** @brief Return the bitwise and of two bitvectors.
     * @pre @a x.size() == size() */
    Bitvector operator&(const Bitvector &x) const;

    /** @brief Return the bitwise or of two bitvectors.
     * @pre @a x.size() == size() */
    Bitvector operator|(const Bitvector &x) const;

    /** @brief Return the bitwise exclusive or of two bitvectors.
     * @pre @a x.size() == size() */
    Bitvector operator^(const Bitvector &x) const;

    /** @brief Return the bitwise subtraction of two bitvectors.
     * @pre @a x.size() == size()
     *
     * <code>x - y</code> is equivalent to <code>x & ~y</code>. */
    Bitvector operator-(const Bitvector &x) const;


    /** @brief Negate this bitvector by flipping each of its bits. */
    void negate();

    /** @brief Modify this bitvector by bitwise and with @a x.
     * @pre @a x.size() == size()
     * @return *this */
    Bitvector &operator&=(const Bitvector &x);

    /** @brief Modify this bitvector by bitwise or with @a x.
     * @pre @a x.size() == size()
     * @return *this */
    Bitvector &operator|=(const Bitvector &x);

    /** @brief Modify this bitvector by bitwise exclusive or with @a x.
     * @pre @a x.size() == size()
     * @return *this */
    Bitvector &operator^=(const Bitvector &x);

    /** @brief Modify this bitvector by bitwise subtraction with @a x.
     * @pre @a x.size() == size()
     * @return *this
     *
     * Equivalent to <code>*this &= ~@a x</code>. */
    Bitvector &operator-=(const Bitvector &x);


    /** @brief Modify this bitvector by bitwise or with an offset @a x.
     * @param x bitwise or operand
     * @param offset initial offset
     * @pre @a offset >= 0 && @a offset + @a x.size() <= size()
     *
     * Logically shifts @a x to start at position @a offset, then performs
     * a bitwise or.  <code>a.offset_or(b, offset)</code> is equivalent to:
     * @code
     * for (int i = 0; i < b.size(); ++i)
     *     a[offset+i] |= b[i];
     * @endcode */
    void offset_or(const Bitvector &x, int offset);

    /** @brief Modify this bitvector by bitwise or, returning difference.
     * @param x bitwise or operand
     * @param[out] difference set to (@a x - old *this)
     * @pre @a x.size() == size()
     * @post @a difference.size() == size()
     * @post @a x | *this == *this
     * @post (@a difference & *this & @a x) == @a difference
     *
     * Same as operator|=, but additionally preserves any change in this
     * bitvector.  Any newly set bits are returned in @a difference. */
    void or_with_difference(const Bitvector &x, Bitvector &difference);


    /** @brief Return whether this bitvector and @a x have a common true bit.
     *
     * This bitvector and @a x may have different sizes; the smaller is used. */
    bool nonzero_intersection(const Bitvector &x) const;


    /** @brief Swap the contents of this bitvector and @a x. */
    void swap(Bitvector &x);


    /** @cond never */
    typedef word_type data_word_type CLICK_DEPRECATED;
    enum { data_word_bits = word_bits };
    inline word_type *data_words() CLICK_DEPRECATED;
    inline const word_type *data_words() const CLICK_DEPRECATED;
    /** @endcond never */

  private:

    enum { MAX_INLINE_BIT = 63, MAX_INLINE_WORD = 1 };

    int _max;
    uint32_t *_data;
    uint32_t _f0;
    uint32_t _f1;

    void finish_copy_constructor(const Bitvector &);
    void clear_last();
    void resize_to_max(int, bool);

};


/** @class Bitvector::Bit
  @brief A wrapper class that acts like a single bit.

  Bits are returned by modifiable Bitvectors' operator[].  They act like bools,
  but Bit operations actually index into individual bits in some shared word. */
class Bitvector::Bit { public:

    /** @brief Construct a bit at offset @a bit_offset in data word @a w. */
    Bit(Bitvector::word_type &w, int bit_offset)
	: _p(w), _mask(Bitvector::word_type(1) << bit_offset) {
    }

    typedef Bitvector::unspecified_bool_type unspecified_bool_type;

    /** @brief Check if this bit is true. */
    inline operator unspecified_bool_type() const {
	return (_p & _mask) != 0 ? &Bitvector::zero : 0;
    }

    /** @brief Set this bit to @a x. */
    Bit &operator=(bool x) {
	if (x)
	    _p |= _mask;
	else
	    _p &= ~_mask;
	return *this;
    }

    /** @overload */
    Bit &operator=(const Bit &x) {
	if (x._p & x._mask)
	    _p |= _mask;
	else
	    _p &= ~_mask;
	return *this;
    }

    /** @brief Modify this bit by bitwise and with @a x. */
    Bit &operator&=(bool x) {
	if (!x)
	    _p &= ~_mask;
	return *this;
    }

    /** @brief Modify this bit by bitwise or with @a x. */
    Bit &operator|=(bool x) {
	if (x)
	    _p |= _mask;
	return *this;
    }

    /** @brief Modify this bit by bitwise exclusive or with @a x. */
    Bit &operator^=(bool x) {
	if (x)
	    _p ^= _mask;
	return *this;
    }

    /** @brief Modify this bit by bitwise subtraction with @a x. */
    Bit &operator-=(bool x) {
	if (x)
	    _p &= ~_mask;
	return *this;
    }

  private:

    Bitvector::word_type &_p;
    Bitvector::word_type _mask;

};


inline
Bitvector::Bitvector(int n)
    : _max(n - 1), _data(&_f0), _f0(0), _f1(0)
{
    assert(n >= 0);
    if (_max > MAX_INLINE_BIT)
	resize_to_max(_max, false);
}

inline
Bitvector::Bitvector(int n, bool b)
    : _max(n - 1), _data(&_f0), _f0(0), _f1(0)
{
    assert(n >= 0);
    if (_max > MAX_INLINE_BIT)
	resize_to_max(_max, false);
    if (b)
	assign(n, b);
}

inline
Bitvector::Bitvector(const Bitvector &o)
    : _max(o._max), _data(&_f0), _f0(o._data[0]), _f1(o._data[1])
{
    if (_max > MAX_INLINE_BIT)
	finish_copy_constructor(o);
}

inline void
Bitvector::resize(int n)
{
    assert(n >= 0);
    if (n - 1 > MAX_INLINE_BIT)
	resize_to_max(n - 1, true);
    _max = n - 1;
}

inline Bitvector::Bit
Bitvector::operator[](int i)
{
    assert(i >= 0 && i <= _max);
    return Bit(_data[i>>wshift], i & wmask);
}

inline bool
Bitvector::operator[](int i) const
{
    assert(i >= 0 && i <= _max);
    return (_data[i>>wshift] & (word_type(1) << (i & wmask))) != 0;
}

inline Bitvector::Bit
Bitvector::force_bit(int i)
{
    assert(i >= 0);
    if (i > _max)
	resize(i + 1);
    return Bit(_data[i>>wshift], i & wmask);
}

/** @brief Return the number of valid data words. */
inline int Bitvector::word_size() const {
    return (_max + wbits) >> wshift;
}

/** @brief Return the index of the maximum valid data word. */
inline int Bitvector::max_word() const {
    return (_max < 0 ? -1 : _max >> wshift);
}

/** @brief Return a pointer to this bitvector's data words. */
inline Bitvector::word_type *Bitvector::words() {
    return _data;
}

/** @overload */
inline const Bitvector::word_type *Bitvector::words() const {
    return _data;
}

/** @cond never */
inline Bitvector::word_type *Bitvector::data_words() {
    return _data;
}
inline const Bitvector::word_type *Bitvector::data_words() const {
    return _data;
}
/** @endcond never */

inline Bitvector &
Bitvector::operator-=(const Bitvector &o)
{
    return *this &= ~o;
}

inline Bitvector
Bitvector::operator~() const
{
    Bitvector m = *this;
    m.negate();
    return m;
}

inline Bitvector
Bitvector::operator&(const Bitvector &o) const
{
    Bitvector m = *this;
    m &= o;
    return m;
}

inline Bitvector
Bitvector::operator|(const Bitvector &o) const
{
    Bitvector m = *this;
    m |= o;
    return m;
}

inline Bitvector
Bitvector::operator^(const Bitvector &o) const
{
    Bitvector m = *this;
    m ^= o;
    return m;
}

inline Bitvector
Bitvector::operator-(const Bitvector &o) const
{
    return *this & ~o;
}

inline void click_swap(Bitvector &a, Bitvector &b)
{
    a.swap(b);
}

inline void assign_consume(Bitvector &a, Bitvector &b)
{
    a.swap(b);
}

CLICK_ENDDECLS
#endif
