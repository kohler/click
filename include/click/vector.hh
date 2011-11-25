#ifndef CLICK_VECTOR_HH
#define CLICK_VECTOR_HH
#include <click/algorithm.hh>
#include <click/array_memory.hh>
CLICK_DECLS

/** @file <click/vector.hh>
  @brief Click's vector container template. */

/** @cond never */
template <typename AM> class vector_memory { public:
    typedef int size_type;
    typedef typename AM::type type;
    typedef type *iterator;
    inline bool need_argument_copy(const type *argp) {
	return fast_argument<type>::is_reference && (uintptr_t) argp - (uintptr_t) l_ < (size_t) (n_ * sizeof(type));
    }

    vector_memory()
	: l_(0), n_(0), capacity_(0) {
    }
    ~vector_memory();
    void assign(const vector_memory<AM> &x);
    void assign(size_type n, const type *vp);
    void resize(size_type n, const type *vp);
    iterator begin() {
	return l_;
    }
    iterator end() {
	return l_ + n_;
    }
    iterator insert(iterator it, const type *vp);
    iterator erase(iterator a, iterator b);
    inline void push_back(const type *vp) {
	if (n_ < capacity_) {
	    AM::mark_undefined(l_ + n_, 1);
	    AM::fill(l_ + n_, 1, vp);
	    ++n_;
	} else
	    reserve_and_push_back(-1, vp);
    }
    inline void pop_back() {
	assert(n_ > 0);
	--n_;
	AM::destroy(l_ + n_, 1);
	AM::mark_noaccess(l_ + n_, 1);
    }
    inline void clear() {
	AM::destroy(l_, n_);
	AM::mark_noaccess(l_, n_);
	n_ = 0;
    }
    bool reserve_and_push_back(size_type n, const type *vp);
    void swap(vector_memory<AM> &x);
    type *l_;
    size_type n_;
    size_type capacity_;
};
/** @endcond never */


/** @class Vector
  @brief Vector template.

  Vector implements a vector, or growable array, suitable for use in the
  kernel or at user level. Its interface should be compatible with C++'s
  std::vector, although that type has more methods. Vector elements are
  accessed with operator[] like arrays, and can be resized and expanded
  through append operations (see push_back() and resize()). A common (and
  efficient) usage pattern grows a Vector through repeated push_back() calls.

  Vector iterators are pointers into the underlying array. This can simplify
  interactions between Vector and code that expects conventional C arrays.

  Vector's push_front() and pop_front() operations are quite expensive (O(size())
  complexity). For fast push_front() and pop_front() operations, use Deque.

  Example code:
  @code
  Vector<int> v;
  printf("%d\n", v.size());         // prints "0"

  v.push_back(1);
  v.push_back(2);
  printf("%d\n", v.size());         // prints "2"
  printf("%d %d\n", v[0], v[1]);    // prints "1 2"

  Vector<int>::iterator it = v.begin();
  int *ip = it;                     // Vector iterators are pointers
  assert(it == v.end() - 2);

  v.erase(v.begin());
  printf("%d\n", v.size());         // prints "1"
  printf("%d\n", v[0]);             // prints "2"
  @endcode
*/
template <typename T>
class Vector {

    typedef typename array_memory<T>::type array_memory_type;
    typedef typename vector_memory<array_memory_type>::type memory_object;
    mutable vector_memory<array_memory_type> vm_;

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


    /** @brief Construct an empty vector. */
    explicit Vector() {
    }

    /** @brief Construct a vector containing @a n copies of @a v. */
    explicit Vector(size_type n, value_argument_type v) {
	vm_.resize(n, (const memory_object *)&v);
    }

    /** @brief Construct a vector as a copy of @a x. */
    Vector(const Vector<T> &x) {
	vm_.assign(x.vm_);
    }

    /** @brief Destroy the vector, freeing its memory. */
    ~Vector() {
    }


    /** @brief Return the number of elements. */
    size_type size() const {
	return vm_.n_;
    }

    /** @brief Test if the vector is empty (size() == 0). */
    bool empty() const {
	return vm_.n_ == 0;
    }

    /** @brief Return the vector's capacity.

	The capacity is greater than or equal to the size(). Functions such as
	resize(n) will not allocate new memory for the vector if n <=
	capacity(). */
    size_type capacity() const {
	return vm_.capacity_;
    }


    /** @brief Iterator type. */
    typedef T *iterator;

    /** @brief Const iterator type. */
    typedef const T *const_iterator;

    /** @brief Return an iterator for the first element in the vector. */
    iterator begin() {
	return (iterator) vm_.l_;
    }
    /** @overload */
    const_iterator begin() const	{ return (const_iterator) vm_.l_; }

    /** @brief Return an iterator for the end of the vector.
      @invariant end() == begin() + size() */
    iterator end() {
	return (iterator) vm_.l_ + vm_.n_;
    }
    /** @overload */
    const_iterator end() const		{ return (const_iterator) vm_.l_ + vm_.n_; }


    /** @brief Return a reference to the <em>i</em>th element.
      @pre 0 <= @a i < size() */
    T &operator[](size_type i) {
	assert((unsigned) i < (unsigned) vm_.n_);
	return *(T *)&vm_.l_[i];
    }
    /** @overload */
    const T &operator[](size_type i) const {
	assert((unsigned) i < (unsigned) vm_.n_);
	return *(T *)&vm_.l_[i];
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
    const T &back() const		{ return operator[](vm_.n_ - 1); }

    /** @brief Return a reference to the <em>i</em>th element.
      @pre 0 <= @a i < size()

      Unlike operator[]() and at(), this function does not check bounds,
      even if assertions are enabled. Use with caution. */
    T &unchecked_at(size_type i) {
	return *(T *)&vm_.l_[i];
    }
    /** @overload */
    const T &unchecked_at(size_type i) const	{ return *(T *)&vm_.l_[i]; }

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

    /** @brief Append element @a v.

      A copy of @a v is inserted at position size(). Takes amortized O(1)
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
      position forward. Takes O(size()) time. */
    void push_front(value_argument_type v) {
	vm_.insert(vm_.l_, (const memory_object *) &v);
    }

    /** @brief Remove the first element.

      Other elements are shifted one position backward. Takes O(size())
      time. */
    void pop_front() {
	vm_.erase(vm_.l_, vm_.l_ + 1);
    }

    /** @brief Insert @a v before position @a it.
      @return An iterator pointing at the new element. */
    iterator insert(iterator it, value_argument_type v) {
	return (iterator) vm_.insert((memory_object *) it, (const memory_object *) &v);
    }

    /** @brief Remove the element at position @a it.
      @return An iterator pointing at the element following @a it. */
    iterator erase(iterator it) {
	return (it < end() ? erase(it, it + 1) : it);
    }

    /** @brief Remove the elements in [@a a, @a b).
      @return An iterator corresponding to @a b. */
    iterator erase(iterator a, iterator b) {
	return (iterator) vm_.erase((memory_object *) a, (memory_object *) b);
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
    bool reserve(size_type n)		{ return vm_.reserve_and_push_back(n, 0); }


    /** @brief Swap the contents of this vector and @a x. */
    void swap(Vector<T> &x)		{ vm_.swap(x.vm_); }


    /** @brief Replace this vector's contents with a copy of @a x. */
    Vector<T> &operator=(const Vector<T> &x) {
	vm_.assign(x.vm_);
	return *this;
    }

    /** @brief Replace this vector's contents with @a n copies of @a v.
      @post size() == @a n */
    Vector<T> &assign(size_type n, value_argument_type v = T()) {
	vm_.assign(n, (const memory_object *) &v);
	return *this;
    }

};

/** @cond never */
template <typename T> T &Vector<T>::at_u(size_type i) {
    return unchecked_at(i);
}
template <typename T> const T &Vector<T>::at_u(size_type i) const {
    return unchecked_at(i);
}
/** @endcond never */

template <typename T>
inline void click_swap(Vector<T> &a, Vector<T> &b)
{
    a.swap(b);
}

template <typename T>
inline void assign_consume(Vector<T> &a, Vector<T> &b)
{
    a.swap(b);
}

CLICK_ENDDECLS
#include <click/vector.cc>
#endif
