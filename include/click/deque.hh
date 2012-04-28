#ifndef CLICK_DEQUE_HH
#define CLICK_DEQUE_HH 1
#include <click/algorithm.hh>
#include <click/array_memory.hh>
CLICK_DECLS

/** @cond never */
template <typename AM> class deque_memory { public:
    typedef int size_type;
    typedef typename AM::type type;
    inline bool need_argument_copy(const type *argp) const {
	return fast_argument<type>::is_reference && (uintptr_t) argp - (uintptr_t) l_ < (size_t) (capacity_ * sizeof(type));
    }
    inline size_type canonp(size_type p) const {
	return p < capacity_ ? p : p - capacity_;
    }
    inline size_type i2p(size_type i) const {
	return canonp(head_ + i);
    }
    inline size_type prevp(size_type p) const {
	return p ? p - 1 : capacity_ - 1;
    }
    inline size_type nextp(size_type p) const {
	return p + 1 != capacity_ ? p + 1 : 0;
    }
    inline size_type naccess(size_type n) const {
	return head_ + n <= capacity_ ? n : capacity_ - head_;
    }

    deque_memory()
	: l_(0), head_(0), n_(0), capacity_(0) {
    }
    ~deque_memory();
    void assign(const deque_memory<AM> &x);
    void assign(size_type n, const type *vp);
    void resize(size_type n, const type *vp);
    bool insert(int i, const type *vp);
    int erase(int ai, int bi);
    inline void push_back(const type *vp) {
	if (n_ < capacity_) {
	    size_type p = i2p(n_);
	    AM::mark_undefined(l_ + p, 1);
	    AM::fill(l_ + p, 1, vp);
	    ++n_;
	} else
	    reserve_and_push(-1, false, vp);
    }
    inline void pop_back() {
	assert(n_ > 0);
	--n_;
	size_type p = i2p(n_);
	AM::destroy(l_ + p, 1);
	AM::mark_noaccess(l_ + p, 1);
    }
    inline void push_front(const type *vp) {
	if (n_ < capacity_) {
	    head_ = prevp(head_);
	    AM::mark_undefined(l_ + head_, 1);
	    AM::fill(l_ + head_, 1, vp);
	    ++n_;
	} else
	    reserve_and_push(-1, true, vp);
    }
    inline void pop_front() {
	assert(n_ > 0);
	--n_;
	AM::destroy(l_ + head_, 1);
	AM::mark_noaccess(l_ + head_, 1);
	head_ = nextp(head_);
    }
    inline void clear() {
	size_type f = naccess(n_);
	AM::destroy(l_ + head_, f);
	AM::destroy(l_, n_ - f);
	AM::mark_noaccess(l_, capacity_);
	head_ = n_ = 0;
    }
    bool reserve_and_push(size_type n, bool isfront, const type *vp);
    void swap(deque_memory<AM> &x);
    type *l_;
    size_type head_;
    size_type n_;
    size_type capacity_;
};
/** @endcond never */

template <typename T> class Deque_iterator;
template <typename T> class Deque_const_iterator;

/** @class Deque
  @brief Deque template.

  Deque implements a double-ended queue: a growable array that can efficiently
  add and remove elements at both ends (see push_back(), pop_back(),
  push_front(), and pop_front()). Its interface should be compatible with
  C++'s std::deque, although that type has more methods. Deque elements are
  accessed with operator[] like arrays.

  Deque is implemented using a circular buffer of elements. This makes its
  operations slightly slower than those of Vector. If you only need to push
  elements to the end of an array, prefer Vector.

  Example code:
  @code
  Deque<int> d;
  printf("%d\n", d.size());         // prints "0"

  d.push_back(1);
  d.push_back(2);
  printf("%d\n", d.size());         // prints "2"
  printf("%d %d\n", d[0], d[1]);    // prints "1 2"

  d.push_front(0);
  d.push_front(-1);
  printf("%d\n", d.size());         // prints "4"
  printf("%d %d %d %d\n", d[0], d[1], d[2], d[3]);
                                    // prints "-1 0 1 2"

  d.pop_front();
  d.pop_back();
  printf("%d\n", d.size());         // prints "2"
  printf("%d %d\n", d[0], d[1]);    // prints "0 1"
  @endcode
*/
template <typename T>
class Deque {

    typedef typename array_memory<T>::type array_memory_type;
    typedef typename deque_memory<array_memory_type>::type memory_object;
    mutable deque_memory<array_memory_type> vm_;

  public:

    /** @brief Value type. */
    typedef T value_type;

    /** @brief Reference to value type. */
    typedef T &reference;

    /** @brief Const reference to value type. */
    typedef const T &const_reference;

    /** @brief Pointer to value type. */
    typedef T *pointer;

    /** @brief Pointer to const value type. */
    typedef const T *const_pointer;

    /** @brief Type used for value arguments (either T or const T &). */
    typedef typename fast_argument<T>::type value_argument_type;

    typedef const T &const_access_type;

    /** @brief Type of sizes (size()). */
    typedef int size_type;

    /** @brief Constant passed to reserve() to grow the Vector. */
    enum { RESERVE_GROW = (size_type) -1 };


    /** @brief Construct an empty deque. */
    explicit Deque() {
    }

    /** @brief Construct a deque containing @a n copies of @a v. */
    explicit Deque(size_type n, value_argument_type v) {
	vm_.resize(n, (const memory_object *)&v);
    }

    /** @brief Construct a deque as a copy of @a x. */
    Deque(const Deque<T> &x) {
	vm_.assign(x.vm_);
    }

    /** @brief Destroy the deque, freeing its memory. */
    ~Deque() {
    }


    /** @brief Return the number of elements. */
    size_type size() const {
	return vm_.n_;
    }

    /** @brief Test if the deque is empty (size() == 0). */
    bool empty() const {
	return vm_.n_ == 0;
    }

    /** @brief Return the deque's capacity.

	The capacity is greater than or equal to the size(). Functions such as
	resize(n) will not allocate new memory for the deque if n <=
	capacity(). */
    size_type capacity() const {
	return vm_.capacity_;
    }


    /** @brief Const iterator type. */
    typedef Deque_const_iterator<T> const_iterator;

    /** @brief Iterator type. */
    typedef Deque_iterator<T> iterator;

    /** @brief Return an iterator for the first element in the deque. */
    iterator begin() {
	return iterator(this, 0);
    }
    /** @overload */
    const_iterator begin() const	{ return const_iterator(this, 0); }

    /** @brief Return an iterator for the end of the vector.
      @invariant end() == begin() + size() */
    iterator end() {
	return iterator(this, vm_.n_);
    }
    /** @overload */
    const_iterator end() const		{ return const_iterator(this, vm_.n_); }


    /** @brief Return a reference to the <em>i</em>th element.
      @pre 0 <= @a i < size() */
    T &operator[](size_type i) {
	assert((unsigned) i < (unsigned) vm_.n_);
	return *(T *)&vm_.l_[vm_.i2p(i)];
    }
    /** @overload */
    const T &operator[](size_type i) const {
	assert((unsigned) i < (unsigned) vm_.n_);
	return *(T *)&vm_.l_[vm_.i2p(i)];
    }
    /** @brief Return a reference to the <em>i</em>th element.
      @pre 0 <= @a i < size()
      @sa operator[]() */
    T &at(size_type i) {
	return operator[](i);
    }
    /** @overload */
    const T &at(size_type i) const	{ return operator[](i); }

    /** @brief Return a reference to the first element.
      @pre !empty() */
    T &front() {
	return operator[](0);
    }
    /** @overload */
    const T &front() const		{ return operator[](0); }

    /** @brief Return a reference to the last element (number size()-1).
      @pre !empty() */
    T &back() {
	return operator[](vm_.n_ - 1);
    }
    /** @overload */
    const T &back() const {
	return operator[](vm_.n_ - 1);
    }

    /** @brief Return a reference to the <em>i</em>th element.
      @pre 0 <= @a i < size()

      Unlike operator[]() and at(), this function does not check bounds,
      even if assertions are enabled. Use with caution. */
    T &unchecked_at(size_type i) {
	return *(T *)&vm_.l_[vm_.i2p(i)];
    }
    /** @overload */
    const T &unchecked_at(size_type i) const {
	return *(T *)&vm_.l_[vm_.i2p(i)];
    }

    /** @cond never */
    inline T &at_u(size_type i) CLICK_DEPRECATED;
    inline const T &at_u(size_type i) const CLICK_DEPRECATED;
    /** @endcond never */


    /** @brief Resize the vector to contain @a n elements.
      @param n new size
      @param v value used to fill new elements */
    void resize(size_type n, value_argument_type v = T()) {
	vm_.resize(n, (memory_object *) &v);
    }

    /** @brief Append @a v to the end of the vector.

      A copy of @a v is added to position size(). Takes amortized O(1)
      time. */
    void push_back(value_argument_type v) {
	vm_.push_back((const memory_object *) &v);
    }

    /** @brief Remove the last element.

      Takes O(1) time. */
    void pop_back() {
	vm_.pop_back();
    }

    /** @brief Prepend element @a v.

      A copy of @a v is added to position 0. Other elements are shifted one
      position forward. Takes amortized O(1) time. */
    void push_front(value_argument_type v) {
	vm_.push_front((const memory_object *) &v);
    }

    /** @brief Remove the first element.

      Other elements are shifted one position backward. Takes O(1) time. */
    void pop_front() {
	vm_.pop_front();
    }

    /** @brief Insert @a v before position @a it.
      @return An iterator pointing at the new element. */
    iterator insert(iterator it, value_argument_type v) {
	assert(it.q_ == this);
	if (likely(vm_.insert(it.p_, (const memory_object *) &v)))
	    return it;
	else
	    return end();
    }

    /** @brief Remove the element at position @a it.
      @return An iterator pointing at the element following @a it. */
    iterator erase(iterator it) {
	assert(it.q_ == this);
	return (it < end() ? erase(it, it + 1) : it);
    }

    /** @brief Remove the elements in [@a a, @a b).
      @return An iterator corresponding to @a b. */
    iterator erase(iterator a, iterator b) {
	assert(a.q_ == this && b.q_ == this);
	return iterator(this, vm_.erase(a.p_, b.p_));
    }

    /** @brief Remove all elements.
      @post size() == 0 */
    void clear() {
	vm_.clear();
    }


    /** @brief Reserve space for at least @a n more elements.
      @return true iff reserve succeeded.

      This function changes the vector's capacity(), not its size(). If
      reserve(@a n) succeeds, then any succeeding call to resize(@a m) with @a
      m < @a n will succeed without allocating vector memory. */
    bool reserve(size_type n)		{ return vm_.reserve_and_push(n, false, 0); }


    /** @brief Swap the contents of this deque and @a x. */
    void swap(Deque<T> &x)		{ vm_.swap(x.vm_); }


    /** @brief Replace this deque's contents with a copy of @a x. */
    Deque<T> &operator=(const Deque<T> &x) {
	vm_.assign(x.vm_);
	return *this;
    }

    /** @brief Replace this deque's contents with @a n copies of @a v.
      @post size() == @a n */
    Deque<T> &assign(size_type n, value_argument_type v = T()) {
	vm_.assign(n, (const memory_object *) &v);
	return *this;
    }

};

template <typename T>
class Deque_const_iterator {
    const Deque<T> *q_;
    typename Deque<T>::size_type p_;
    friend class Deque<T>;
  public:
    typedef typename Deque<T>::size_type size_type;
    typedef Deque_const_iterator<T> const_iterator;
    typedef Deque_iterator<T> iterator;

    Deque_const_iterator() {
    }
    Deque_const_iterator(const Deque<T> *q, size_type p)
	: q_(q), p_(p) {
    }
    const T *operator->() const {
	return &(*q_)[p_];
    }
    const T &operator*() const {
	return (*q_)[p_];
    }
    const T &operator[](size_type i) const {
	return (*q_)[p_ + i];
    }
    bool operator==(const const_iterator &x) const {
	return p_ == x.p_ && q_ == x.q_;
    }
    bool operator!=(const const_iterator &x) const {
	return p_ != x.p_ || q_ != x.q_;
    }
    bool operator<(const const_iterator &x) const {
	assert(q_ == x.q_);
	return p_ < x.p_;
    }
    bool operator<=(const const_iterator &x) const {
	assert(q_ == x.q_);
	return p_ <= x.p_;
    }
    bool operator>=(const const_iterator &x) const {
	assert(q_ == x.q_);
	return p_ >= x.p_;
    }
    bool operator>(const const_iterator &x) const {
	assert(q_ == x.q_);
	return p_ > x.p_;
    }
    size_type diff(const const_iterator &x) const {
	assert(q_ == x.q_);
	return p_ - x.p_;
    }
    void operator++(int) {
	++p_;
    }
    const_iterator &operator++() {
	++p_;
	return *this;
    }
    void operator--(int) {
	--p_;
    }
    const_iterator &operator--() {
	--p_;
	return *this;
    }
    const_iterator &operator+=(size_type n) {
	p_ += n;
	return *this;
    }
    const_iterator &operator-=(size_type n) {
	p_ -= n;
	return *this;
    }
};

template <typename T>
class Deque_iterator : public Deque_const_iterator<T> { public:
    typedef typename Deque<T>::size_type size_type;
    typedef Deque_const_iterator<T> const_iterator;
    typedef Deque_iterator<T> iterator;

    Deque_iterator() {
    }
    Deque_iterator(Deque<T> *q, int p)
	: const_iterator(q, p) {
    }
    T *operator->() const {
	return const_cast<T *>(const_iterator::operator->());
    }
    T &operator*() const {
	return const_cast<T &>(const_iterator::operator*());
    }
    T &operator[](int n) const {
	return const_cast<T &>(const_iterator::operator[](n));
    }
    iterator &operator+=(size_type n) {
	const_iterator::operator+=(n);
	return *this;
    }
    iterator &operator-=(size_type n) {
	const_iterator::operator-=(n);
	return *this;
    }
};

template <typename T>
Deque_const_iterator<T> operator+(Deque_const_iterator<T> it, typename Deque<T>::size_type n)
{
    return it += n;
}

template <typename T>
Deque_const_iterator<T> operator-(Deque_const_iterator<T> it, typename Deque<T>::size_type n)
{
    return it -= n;
}

template <typename T>
Deque_iterator<T> operator+(Deque_iterator<T> it, typename Deque<T>::size_type n)
{
    return it += n;
}

template <typename T>
Deque_iterator<T> operator-(Deque_iterator<T> it, typename Deque<T>::size_type n)
{
    return it -= n;
}

template <typename T>
typename Deque_const_iterator<T>::size_type operator-(const Deque_const_iterator<T> &a,
						      const Deque_const_iterator<T> &b)
{
    return a.diff(b);
}

template <typename T>
inline void click_swap(Deque<T> &a, Deque<T> &b)
{
    a.swap(b);
}

template <typename T>
inline void assign_consume(Deque<T> &a, Deque<T> &b)
{
    a.swap(b);
}

CLICK_ENDDECLS
#include <click/deque.cc>
#endif
