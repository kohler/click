#ifndef ALIGNMENT_HH
#define ALIGNMENT_HH
#include <click/string.hh>
class ElementT;

class Alignment { public:

    /** @brief Construct the empty alignment. */
    Alignment()
	: _modulus(empty_modulus), _offset(0) {
    }
    /** @brief Construct the alignment @a modulus/@a offset.
     * @pre @a modulus > 0 && 0 <= @a offset < @a modulus */
    Alignment(int modulus, int offset)
	: _modulus(modulus), _offset(offset) {
	assert(modulus > 0 && offset >= 0 && offset < modulus);
    }
    /** @brief Construct an alignment from an Align element's configuration. */
    Alignment(ElementT *element);
    /** @brief Return a bad alignment.
     *
     * A bad alignment represents unsatisfiable alignment constraints,
     * such as Alignment(4, 1) & Alignment(4, 2). */
    static inline Alignment make_bad() {
	return Alignment(bad_modulus, 0, 0);
    }
    /** @brief Return a universal alignment.
     *
     * A universal alignment matches any alignment specification.  It is
     * generally used for impossible paths: if an element will not emit
     * any packets on some output, then that output's packets have
     * universal alignment, because all emitted packets trivially match any
     * downstream alignment requirements. */
    static inline Alignment make_universal() {
	return Alignment(universal_modulus, 0, 0);
    }

    /** @brief Return this alignment's modulus.
     *
     * Returns 1 for bad, empty, and universal alignments. */
    int modulus() const {
	return (_modulus > 1 ? _modulus : 1);
    }
    /** @brief Return this alignment's offset.
     *
     * This is always < modulus().  Returns 0 for bad, empty, and universal
     * alignments. */
    int offset() const {
	return _offset;
    }
    /** @brief Test if this alignment is empty. */
    bool empty() const {
	return _modulus == empty_modulus;
    }
    /** @brief Test if this alignment is bad. */
    bool bad() const {
	return _modulus == bad_modulus;
    }
    /** @brief Test if this alignment is universal. */
    bool universal() const {
	return _modulus == universal_modulus;
    }

    /** @brief Return true iff *this and @a x represent the same alignment. */
    bool operator==(const Alignment &x) const;
    /** @brief Return true iff *this and @a x represent different alignments. */
    bool operator!=(const Alignment &x) const;

    /** @brief Return true iff *this is at least as strong as alignment @a x.
     *
     * This alignment is at least as strong as @a x iff every packet aligned
     * to this alignment is also aligned to @a x's alignment.  For instance,
     * Alignment(4, 2) <= Alignment(2, 0). */
    bool operator<=(const Alignment &x) const;

    /** @brief Shift this alignment forward by @a offset bytes.
     *
     * For example, Alignment(4, 2) + 3 == Alignment(4, 1). */
    Alignment &operator+=(int offset);
    /** @brief Shift this alignment backward by @a offset bytes.
     *
     * For example, Alignment(4, 2) - 3 == Alignment(4, 3). */
    Alignment &operator-=(int offset);

    /** @brief Intersect this alignment with @a x.
     * @return *this
     *
     * The result is the weakest alignment for which all matching packets
     * match both @a x and *this.  If no such alignment exists, the result is
     * bad; for example, (Alignment(4, 0) & Alignment(4, 2)).bad() is true.
     *
     * Neither *this nor @a x may be universal.
     *
     * If *this is empty, then *this is set to @a x.  If @a x is empty, then
     * *this remains unchanged. */
    Alignment &operator&=(const Alignment &x);

    /** @brief Union this alignment with @a x.
     * @return *this
     *
     * The result is the weakest alignment that matches any packets that
     * match either @a x or *this.
     *
     * If *this is empty, then *this is set to @a x.  If @a x is empty, then
     * *this remains unchanged.  If *this is universal and @a x is a real
     * alignment, then *this is set to @a x.  (See make_universal() to
     * understand why this makes sense.) */
    Alignment &operator|=(const Alignment &x);

    /** @brief Return a string representation of this alignment. */
    String unparse() const;

    static void test();

  private:

    enum {
	empty_modulus = 0,
	bad_modulus = -1,
	universal_modulus = -2
    };

    int _modulus;
    int _offset;

    Alignment(int m, int o, int)
	: _modulus(m), _offset(o) {
    }

};

inline bool
Alignment::operator==(const Alignment &x) const
{
    return _modulus == x._modulus && _offset == x._offset;
}

inline bool
Alignment::operator!=(const Alignment &x) const
{
    return _modulus != x._modulus || _offset != x._offset;
}

inline Alignment &
Alignment::operator-=(int offset)
{
    return *this += (-offset);
}

inline Alignment
operator+(Alignment a, int offset)
{
    return a += offset;
}

inline Alignment
operator-(Alignment a, int offset)
{
    return a += (-offset);
}

inline Alignment
operator|(Alignment a, const Alignment &b)
{
    return a |= b;
}

inline Alignment
operator&(Alignment a, const Alignment &b)
{
    return a &= b;
}

#endif
