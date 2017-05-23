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
class Bitvector {
  public:

    class Bit;
    typedef bool (Bitvector::*unspecified_bool_type)() const;

    typedef uint32_t word_type;
    enum { wbits = 32, wshift = 5, wmask = 31 };

    inline Bitvector();
    explicit inline Bitvector(int n);
    explicit inline Bitvector(bool bit);
    inline Bitvector(int n, bool bit);
    inline Bitvector(const Bitvector &x);
    inline ~Bitvector();

    inline int size() const;
    inline Bit operator[](int i);
    inline bool operator[](int i) const;
    inline Bit force_bit(int i);

    inline int word_size() const;
    inline int max_word() const;
    inline word_type *words();
    inline const word_type *words() const;

    bool zero() const;
    inline operator unspecified_bool_type() const;
    inline bool operator!() const;

    Bitvector &assign(int n, bool bit);
    Bitvector &operator=(const Bitvector &x);
    void clear();
    void resize(int n);

    friend inline bool operator==(const Bitvector &a, const Bitvector &b);
    friend inline bool operator!=(const Bitvector &a, const Bitvector &b);
    bool nonzero_intersection(const Bitvector &x) const;

    inline Bitvector operator~() const;
    friend inline Bitvector operator&(Bitvector a, const Bitvector &b);
    friend inline Bitvector operator|(Bitvector a, const Bitvector &b);
    friend inline Bitvector operator^(Bitvector a, const Bitvector &b);
    friend inline Bitvector operator-(Bitvector a, const Bitvector &b);

    void flip();
    inline void negate();
    Bitvector &operator&=(const Bitvector &x);
    Bitvector &operator|=(const Bitvector &x);
    Bitvector &operator^=(const Bitvector &x);
    inline Bitvector &operator-=(const Bitvector &x);
    void offset_or(const Bitvector &x, int offset);
    void or_with_difference(const Bitvector &x, Bitvector &difference);

    void swap(Bitvector &x);

    inline int weight();

    /** @cond never */
    typedef word_type data_word_type CLICK_DEPRECATED;
    enum { data_word_bits = wbits };
    inline word_type *data_words() CLICK_DEPRECATED;
    inline const word_type *data_words() const CLICK_DEPRECATED;
    /** @endcond never */

  private:

    enum { ninline = 2, inlinebits = ninline * wbits };

    int _max;
    word_type *_data;
    word_type _f[ninline];

    void finish_copy_constructor(const Bitvector &);
    inline void clear_last();
    void hard_resize(int, bool);

};


/** @class Bitvector::Bit
  @brief A wrapper class that acts like a single bit.

  Bits are returned by modifiable Bitvectors' operator[].  They act like bools,
  but Bit operations actually index into individual bits in some shared word. */
class Bitvector::Bit { public:

    inline Bit(Bitvector::word_type &w, int bit_offset);

    inline operator bool() const;

    inline Bit &operator=(bool x);
    inline Bit &operator=(const Bit &x);
    inline void flip();

    inline Bit &operator&=(bool x);
    inline Bit &operator|=(bool x);
    inline Bit &operator^=(bool x);
    inline Bit &operator-=(bool x);

  private:

    Bitvector::word_type &_p;
    Bitvector::word_type _mask;

};


/** @brief Construct an empty bitvector. */
inline Bitvector::Bitvector()
    : _max(-1), _data(_f) {
    _f[0] = 0;
}

/** @brief Construct an all-false bitvector with @a n elements.
    @pre @a n >= 0 */
inline Bitvector::Bitvector(int n)
    : _data(_f) {
    assert(n >= 0);
    if (n <= inlinebits) {
	_max = n - 1;
	memset(_f, 0, sizeof(_f));
    } else {
	_max = -1;
	resize(n);
    }
}

/** @brief Construct a @a bit-valued length-1 bitvector. */
inline Bitvector::Bitvector(bool bit)
    : _max(0), _data(_f) {
    _f[0] = bit;
}

/** @brief Construct a @a bit-valued length-@a n bitvector.
    @pre @a n >= 0 */
inline Bitvector::Bitvector(int n, bool b)
    : _max(-1), _data(_f) {
    assign(n, b);
}

/** @brief Construct a bitvector as a copy of @a x. */
inline Bitvector::Bitvector(const Bitvector &x)
    : _max(x._max), _data(_f) {
    if (_max < inlinebits)
	memcpy(_data, x._data, ninline * sizeof(word_type));
    else
	finish_copy_constructor(x);
}

/** @brief Destroy a bitvector.

    All outstanding Bit objects become invalid. */
inline Bitvector::~Bitvector() {
    if (_data != _f)
	delete[] _data;
}

/** @brief Return the number of bits in the bitvector. */
inline int Bitvector::size() const {
    return _max + 1;
}

/** @brief Test if the bitvector contains at least one true bit.
    @sa zero() */
inline Bitvector::operator unspecified_bool_type() const {
    return !zero() ? &Bitvector::zero : 0;
}

/** @brief Test if the bitvector contains no true bits.
    @sa zero() */
inline bool Bitvector::operator!() const {
    return zero();
}

/** @brief Return the bit at position @a i.
    @pre 0 <= @a i < size() */
inline Bitvector::Bit Bitvector::operator[](int i) {
    assert(i >= 0 && i <= _max);
    return Bit(_data[i>>wshift], i & wmask);
}

/** @overload */
inline bool Bitvector::operator[](int i) const {
    assert(i >= 0 && i <= _max);
    return (_data[i>>wshift] & (word_type(1) << (i & wmask))) != 0;
}

/** @brief Return the bit at position @a i, extending if necessary.

    If @a i >= size(), then the bitvector is resize()d to length @a i+1,
    which adds false bits to fill out the vector.

    @pre 0 <= @a i
    @post @a i < size() */
inline Bitvector::Bit Bitvector::force_bit(int i) {
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

/** @brief Test bitvectors for equality. */
inline bool operator==(const Bitvector &a, const Bitvector &b) {
    return a.size() == b.size()
	&& memcmp(a.words(), b.words(), a.word_size() * sizeof(Bitvector::word_type)) == 0;
}

/** @brief Test bitvectors for inequality. */
inline bool operator!=(const Bitvector &a, const Bitvector &b) {
    return !(a == b);
}

/** @brief Modify this bitvector by bitwise subtraction with @a x.
    @pre @a x.size() == size()
    @return *this

    Equivalent to <code>*this &= ~@a x</code>. */
inline Bitvector &Bitvector::operator-=(const Bitvector &x) {
    return *this &= ~x;
}

/** @brief Flip all bits in this bitvector.
    @sa negate() */
inline void Bitvector::negate() {
    flip();
}

/** @brief Return the bitwise negation of this bitvector. */
inline Bitvector Bitvector::operator~() const {
    Bitvector m = *this;
    m.flip();
    return m;
}

/** @brief Return the bitwise and of two bitvectors.
    @pre @a a.size() == @a b.size() */
inline Bitvector operator&(Bitvector a, const Bitvector &b) {
    return a &= b;
}

/** @brief Return the bitwise or of two bitvectors.
    @pre @a a.size() == @a b.size() */
inline Bitvector operator|(Bitvector a, const Bitvector &b) {
    return a |= b;
}

/** @brief Return the bitwise exclusive or of two bitvectors.
    @pre @a a.size() == @a b.size() */
inline Bitvector operator^(Bitvector a, const Bitvector &b) {
    return a ^= b;
}

/** @brief Return the bitwise subtraction of two bitvectors.
    @pre @a a.size() == @a b.size()

    <code>a - b</code> is equivalent to <code>a & ~b</code>. */
inline Bitvector operator-(Bitvector a, const Bitvector &b) {
    return a & ~b;
}

inline void click_swap(Bitvector &a, Bitvector &b) {
    a.swap(b);
}

inline void assign_consume(Bitvector &a, Bitvector &b) {
    a.swap(b);
}


/** @brief Construct a bit at offset @a bit_offset in data word @a w. */
inline Bitvector::Bit::Bit(Bitvector::word_type &w, int bit_offset)
    : _p(w), _mask(Bitvector::word_type(1) << bit_offset) {
}

/** @brief Test if this bit is true. */
inline Bitvector::Bit::operator bool() const {
    return (_p & _mask) != 0;
}

/** @brief Set this bit to @a x. */
inline Bitvector::Bit &Bitvector::Bit::operator=(bool x) {
    if (x)
	_p |= _mask;
    else
	_p &= ~_mask;
    return *this;
}

/** @overload */
inline Bitvector::Bit &Bitvector::Bit::operator=(const Bit &x) {
    if (x._p & x._mask)
	_p |= _mask;
    else
	_p &= ~_mask;
    return *this;
}

/** @brief Flip this bit. */
inline void Bitvector::Bit::flip() {
    _p ^= _mask;
}

/** @brief Modify this bit by bitwise and with @a x. */
inline Bitvector::Bit &Bitvector::Bit::operator&=(bool x) {
    if (!x)
	_p &= ~_mask;
    return *this;
}

/** @brief Modify this bit by bitwise or with @a x. */
inline Bitvector::Bit &Bitvector::Bit::operator|=(bool x) {
    if (x)
	_p |= _mask;
    return *this;
}

/** @brief Modify this bit by bitwise exclusive or with @a x. */
inline Bitvector::Bit &Bitvector::Bit::operator^=(bool x) {
    if (x)
	_p ^= _mask;
    return *this;
}

/** @brief Modify this bit by bitwise subtraction with @a x. */
inline Bitvector::Bit &Bitvector::Bit::operator-=(bool x) {
    if (x)
	_p &= ~_mask;
    return *this;
}

/** @brief Return the number of true bits */
inline int Bitvector::weight() {
	int w = 0;
	for (int i = 0; i < size(); i++)
		if ((*this)[i])
			w++;
	return w;
}

CLICK_ENDDECLS
#endif
