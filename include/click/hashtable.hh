#ifndef CLICK_HASHTABLE_HH
#define CLICK_HASHTABLE_HH
/*
 * hashtable.hh -- HashTable template
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software")
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */
#include <click/pair.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
CLICK_DECLS

/** @file <click/hashtable.hh>
 * @brief Click's hash table container template.
 */

template <typename K, typename V = void> class HashTable;
template <typename T> class HashTable_iterator;
template <typename T> class HashTable_const_iterator;

/** @class HashTable
  @brief Hash table template.

  The HashTable template implements a hash table or associative array suitable
  for use in the kernel or at user level.  Its interface is similar to C++'s
  std::map and std::unordered_map, although those types have more methods.

  Used with two template parameters, as HashTable<K, V>, the table maps keys K
  to values V.  Used with one template parameter, as HashTable<T>, HashTable
  is a hash set.  The type T must declare types named key_type and
  key_const_reference.  It also must declare a hashkey() member function
  returning type key_const_reference.  An object's hashkey() defines its key.

  HashTable is a chained hash table.  (Open coding is not
  appropriate in the kernel, where large contiguous memory allocations are
  essentially impossible.)  When run through Google's sparse_hash_table tests
  (April 2008, sparsehash-1.1), HashTable appears to perform slightly better
  than g++'s hash_map, better than sparse_hash_map, and worse than
  dense_hash_map; it takes less memory than hash_map and dense_hash_map.

  HashTable is faster than Click's prior HashMap class and has fewer potential
  race conditions in multithreaded use.  HashMap remains for backward
  compatibility but should not be used in new code.

  @warning HashMap users should beware of HashTable's operator[].  HashMap's
  operator[] returned a non-assignable const reference; it would never add an
  element to the hash table.  In contrast, the HashTable operator[]
  <em>may</em> add an element to the table.  (If added, the element will
  initially have the table's default value.)  For instance, consider:
  @code
  HashMap<String, int> h(0);
  h.insert("A", 1);
  if (!h["B"])      // Nota bene
      printf("B wasn't in table, and still isn't\n");
  for (HashMap<String, int>::iterator it = h.begin(); it; ++it)
      printf("%s -> %d\n", it.key().c_str(), it.value());
                    // Prints  B wasn't in table, and still isn't
                    //         A -> 1
  @endcode

  @warning Here the h["B"] reference does not add an element to
  the hash table, as you can see from the iteration.  Similar HashTable code
  has a different result:
  @code
  HashTable<String, int> h(0);
  h["A"] = 1;
  if (!h["B"])      // Nota bene
      printf("B wasn't in table, but it is now\n");
  for (HashMap<String, int>::iterator it = h.begin(); it; ++it)
      printf("%s -> %d\n", it.key().c_str(), it.value());
                    // Prints  B wasn't in table, but it is now
                    //         A -> 1
                    //         B -> 0
  @endcode

  @warning If you don't want operator[] to add an element, either access
  operator[] through a const hash table, or use get():
  @code
  HashTable<String, int> h(0);
  if (!h.get("B"))
      printf("B wasn't in table, and still isn't\n");
  const HashTable<String, int> &const_h = h;
  if (!const_h["B"])
      printf("B wasn't in table, and still isn't\n");
  @endcode
*/
template <typename T>
class HashTable<T> {

    struct elt {
	T v;
	elt *_hashnext;
	elt(const T &v_)
	    : v(v_) {
	}
	typedef typename T::key_type key_type;
	typedef typename T::key_const_reference key_const_reference;
	key_const_reference hashkey() const {
	    return v.hashkey();
	}
    };

    typedef HashContainer<elt> rep_type;

  public:

    /** @brief Key type. */
    typedef typename T::key_type key_type;

    /** @brief Const reference to key type. */
    typedef typename T::key_const_reference key_const_reference;

    /** @brief Value type.  value_type::key_type must exist. */
    typedef T value_type;

    /** @brief Type of sizes (size(), bucket_count()). */
    typedef typename rep_type::size_type size_type;


    /** @brief Construct an empty hash table. */
    HashTable()
	: _rep() {
    }

    /** @brief Construct a hash table with at least @a n buckets.
     *
     * In kernel modules HashTable has a maximum bucket count, so
     * HashTable(n).bucket_count() might be less than @a n. */
    explicit HashTable(size_type n)
	: _rep(n) {
    }

    /** @brief Construct a hash table as a copy of @a x. */
    HashTable(const HashTable<T> &x)
	: _rep(x._rep.bucket_count()) {
	clone_elements(x);
    }

#if HAVE_CXX_RVALUE_REFERENCES
    /** @overload */
    HashTable(HashTable<T> &&x)
	: _rep() {
	x.swap(*this);
    }
#endif

    /** @brief Destroy the hash table, freeing its memory. */
    ~HashTable();


    /** @brief Return the number of elements. */
    inline size_type size() const {
	return _rep.size();
    }

    /** @brief Test if size() == 0. */
    inline bool empty() const {
	return _rep.empty();
    }

    /** @brief Return the number of buckets. */
    inline size_type bucket_count() const {
	return _rep.bucket_count();
    }

    /** @brief Return the number of elements in hash bucket @a n.
     * @param n bucket number, >= 0 and < bucket_count() */
    inline size_type bucket_size(size_type n) const {
	return _rep.bucket_size(n);
    }


    typedef HashTable_const_iterator<T> const_iterator;
    typedef HashTable_iterator<T> iterator;

    /** @brief Return an iterator for the first element in the table.
     *
     * HashTable iterators return elements in random order. */
    inline iterator begin();
    /** @overload */
    inline const_iterator begin() const;

    /** @brief Return an iterator for the end of the table.
     * @invariant end().live() == false */
    inline iterator end();
    /** @overload */
    inline const_iterator end() const;


    /** @brief Return 1 if an element with key @a key exists, 0 otherwise. */
    inline size_type count(key_const_reference key) const;

    /** @brief Return an iterator for the element with key @a key, if any.
     *
     * Returns end() if no such element exists. */
    inline iterator find(key_const_reference key);
    /** @overload */
    inline const_iterator find(key_const_reference key) const;

    /** @brief Return an iterator for the element with key @a key, if any.
     *
     * Like find(), but additionally moves the found element to the head of
     * its bucket, possibly speeding up future lookups.
     *
     * @note find_prefer() rearranges the ordering of a bucket, and therefore
     * invalidates outstanding iterators. */
    inline iterator find_prefer(key_const_reference key);

    /** @brief Ensure an element with key @a key and return its iterator.
     *
     * If an element with @a key already exists in the table, then, like
     * find(@a key), returns an iterator pointing at at element.  Otherwise,
     * find_insert adds a new value T(@a key) to the table and returns its
     * iterator.
     *
     * @note find_insert() may rebalance the hash table, and thus invalidates
     * outstanding iterators.
     *
     * @sa operator[] */
    inline iterator find_insert(key_const_reference key);

    /** @brief Ensure an element with key @a key and return its reference.
     *
     * If an element with @a key already exists in the table, then returns a
     * reference to that element.  Otherwise, adds a new value T(@a key) to
     * the table and returns a reference to the new element.
     *
     * @note operator[] may rebalance the hash table, and thus invalidates
     * outstanding iterators.
     *
     * @sa find_insert(key_const_reference) */
    inline value_type &operator[](key_const_reference key);

    /** @brief Ensure an element with key @a value.hashkey() and return its iterator.
     *
     * If an element with @a value.hashkey() already exists in the table,
     * then, like find(), returns an iterator pointing at at element.
     * Otherwise, find_insert adds a copy of @a value to the table and returns
     * its iterator.
     *
     * @note find_insert() may rebalance the hash table, and thus invalidates
     * outstanding iterators. */
    inline iterator find_insert(const value_type &value);

    /** @brief Add @a value to the table, replacing any element with that key.
     *
     * Inserts @a value into the table.  If an element with @a value.hashkey()
     * already exists in the table, then it is replaced, and the function
     * returns false.  Otherwise, a copy of @a value is added, and the
     * function returns true.
     *
     * @note set() may rebalance the hash table, and thus invalidates
     * outstanding iterators. */
    bool set(const value_type &value);

    /** @brief Remove the element indicated by @a it.
     * @return An iterator pointing at the next element remaining, or end()
     * if no such element exists. */
    iterator erase(const iterator &it);

    /** @brief Remove any element with @a key.
     *
     * Returns the number of elements removed, which is always 0 or 1. */
    size_type erase(const key_type &key);

    /** @brief Remove all elements.
     * @post size() == 0 */
    void clear();


    /** @brief Swap the contents of this hash table and @a x. */
    void swap(HashTable<T> &x);


    /** @brief Rehash the table, ensuring it contains at least @a n buckets.
     *
     * If @a n < bucket_count(), this function may make the hash table
     * slower. */
    void rehash(size_type n) {
	_rep.rehash(n);
    }


    /** @brief Replace this hash table's contents with a copy of @a x. */
    HashTable<T> &operator=(const HashTable<T> &x);

#if HAVE_CXX_RVALUE_REFERENCES
    /** @overload */
    inline HashTable<T> &operator=(HashTable<T> &&x) {
	x.swap(*this);
	return *this;
    }
#endif

  private:

    rep_type _rep;
    SizedHashAllocator<sizeof(elt)> _alloc;

    void clone_elements(const HashTable<T> &);
    void copy_elements(const HashTable<T> &);

    friend class HashTable_iterator<T>;
    friend class HashTable_const_iterator<T>;
    template <typename K, typename V> friend class HashTable;

};

/** @class HashTable_const_iterator
 * @brief The const_iterator type for HashTable. */
template <typename T>
class HashTable_const_iterator { public:

    /** @brief Construct an uninitialized iterator. */
    HashTable_const_iterator() {
    }

    /** @brief Return a pointer to the element, null if *this == end(). */
    const T *get() const {
	if (_rep)
	    return &_rep.get()->v;
	else
	    return 0;
    }

    /** @brief Return a pointer to the element.
     * @pre *this != end() */
    const T *operator->() const {
	return &_rep.get()->v;
    }

    /** @brief Return a reference to the element.
     * @pre *this != end() */
    const T &operator*() const {
	return _rep.get()->v;
    }

    /** @brief Return this element's key.
     * @pre *this != end() */
    typename HashTable<T>::key_const_reference key() const {
	return _rep.get()->hashkey();
    }

    /** @brief Return true iff *this != end(). */
    bool live() const {
	return (bool) _rep;
    }

    typedef bool (HashTable_const_iterator::*unspecified_bool_type)() const;
    /** @brief Return true iff *this != end(). */
    inline operator unspecified_bool_type() const {
	return _rep ? &HashTable_const_iterator::live : 0;
    }

    /** @brief Advance this iterator to the next element. */
    void operator++(int) {
	_rep++;
    }

    /** @brief Advance this iterator to the next element. */
    void operator++() {
	++_rep;
    }

  private:

    typename HashTable<T>::rep_type::const_iterator _rep;

    inline HashTable_const_iterator(const typename HashTable<T>::rep_type::const_iterator &i)
	: _rep(i) {
    }

    friend class HashTable<T>;
    friend class HashTable_iterator<T>;

};

/** @class HashTable_iterator
  @brief The iterator type for HashTable.

  These iterators apply to HashTable classes that store either a unified
  key-value pair (HashTable<T>), or to HashTable classes that map keys to
  explicit values (HashTable<K, V>).

  Iterators for HashTable<K, V> objects have additional methods.  Given
  HashTable<K, V>::iterator it:

  <ul>
  <li>*it has type Pair<const K, V>.</li>
  <li>it.key() returns the associated key, and is equivalent to it->first.</li>
  <li>it.value() returns the associated value, and is equivalent to
    it->second.</li>
  <li>it.value() is a mutable reference for iterator objects and a const
    reference for const_iterator objects.</li>
  </ul>
*/
template <typename T>
class HashTable_iterator : public HashTable_const_iterator<T> { public:

    typedef HashTable_const_iterator<T> inherited;

    /** @brief Construct an uninitialized iterator. */
    HashTable_iterator() {
    }

    /** @brief Return a pointer to the element, null if *this == end(). */
    T *get() const {
	return const_cast<T *>(inherited::get());
    }

    /** @brief Return a pointer to the element.
     * @pre *this != end() */
    inline T *operator->() const {
	return const_cast<T *>(inherited::operator->());
    }

    /** @brief Return a reference to the element.
     * @pre *this != end() */
    inline T &operator*() const {
	return const_cast<T &>(inherited::operator*());
    }

  private:

    inline HashTable_iterator(const typename HashTable<T>::rep_type::const_iterator &i)
	: inherited(i) {
    }

    friend class HashTable<T>;

};

/** @class HashTable_const_iterator
 * @brief The const_iterator type for HashTable. */
template <typename K, typename V>
class HashTable_const_iterator<Pair<K, V> > { public:

    /** @brief Construct an uninitialized iterator. */
    HashTable_const_iterator() {
    }

    /** @brief Return a pointer to the element, null if *this == end(). */
    const Pair<K, V> *get() const {
	if (_rep)
	    return &_rep.get()->v;
	else
	    return 0;
    }

    /** @brief Return a pointer to the element.
     * @pre *this != end() */
    const Pair<K, V> *operator->() const {
	return &_rep.get()->v;
    }

    /** @brief Return a reference to the element.
     * @pre *this != end() */
    const Pair<K, V> &operator*() const {
	return _rep.get()->v;
    }

    /** @brief Return a reference to the element's key.
     * @pre *this != end()
     * @return operator*().first */
    const K &key() const {
	return operator*().first;
    }

    /** @brief Return a reference to the element's value.
     * @pre *this != end()
     * @return operator*().second */
    const V &value() const {
	return operator*().second;
    }

    /** @brief Return true iff *this != end(). */
    bool live() const {
	return (bool) _rep;
    }

    typedef bool (HashTable_const_iterator::*unspecified_bool_type)() const;
    /** @brief Return true iff *this != end(). */
    inline operator unspecified_bool_type() const {
	return _rep ? &HashTable_const_iterator::live : 0;
    }

    /** @brief Advance this iterator to the next element. */
    void operator++(int) {
	_rep++;
    }

    /** @brief Advance this iterator to the next element. */
    void operator++() {
	++_rep;
    }

  private:

    typename HashTable<Pair<K, V> >::rep_type::const_iterator _rep;

    inline HashTable_const_iterator(const typename HashTable<Pair<K, V> >::rep_type::const_iterator &i)
	: _rep(i) {
    }

    friend class HashTable<Pair<K, V> >;
    friend class HashTable_iterator<Pair<K, V> >;

};

/** @class HashTable_iterator
 * @brief The iterator type for HashTable. */
template <typename K, typename V>
class HashTable_iterator<Pair<K, V> > : public HashTable_const_iterator<Pair<K, V> > { public:

    typedef HashTable_const_iterator<Pair<K, V> > inherited;

    /** @brief Construct an uninitialized iterator. */
    HashTable_iterator() {
    }

    /** @brief Return a pointer to the element, null if *this == end(). */
    Pair<K, V> *get() const {
	return const_cast<Pair<K, V> *>(inherited::get());
    }

    /** @brief Return a pointer to the element, null if *this == end(). */
    inline Pair<K, V> *operator->() const {
	return const_cast<Pair<K, V> *>(inherited::operator->());
    }

    /** @brief Return a reference to the element.
     * @pre *this != end() */
    inline Pair<K, V> &operator*() const {
	return const_cast<Pair<K, V> &>(inherited::operator*());
    }

    /** @brief Return a mutable reference to the element's value.
     * @pre *this != end()
     * @return operator*().second */
    V &value() const {
	return operator*().second;
    }

  private:

    inline HashTable_iterator(const typename HashTable<Pair<K, V> >::rep_type::const_iterator &i)
	: inherited(i) {
    }

    friend class HashTable<Pair<K, V> >;

};


template <typename K, typename V>
class HashTable {

    typedef HashTable<Pair<const K, V> > rep_type;

  public:

    /** @brief Key type. */
    typedef K key_type;

    /** @brief Const reference to key type. */
    typedef const K &key_const_reference;

    /** @brief Value type. */
    typedef V mapped_type;

    /** @brief Pair of key type and value type. */
    typedef Pair<const K, V> value_type;

    /** @brief Type of sizes. */
    typedef typename rep_type::size_type size_type;


    /** @brief Construct an empty hash table with normal default value. */
    HashTable()
	: _rep(), _default_value() {
    }

    /** @brief Construct an empty hash table with default value @a d. */
    explicit HashTable(const mapped_type &d)
	: _rep(), _default_value(d) {
    }

    /** @brief Construct an empty hash table with at least @a n buckets.
     * @param d default value
     * @param n minimum number of buckets */
    HashTable(const mapped_type &d, size_type n)
	: _rep(n), _default_value(d) {
    }

    /** @brief Construct a hash table as a copy of @a x. */
    HashTable(const HashTable<K, V> &x)
	: _rep(x._rep), _default_value(x._default_value) {
    }

#if HAVE_CXX_RVALUE_REFERENCES
    /** @overload */
    HashTable(HashTable<K, V> &&x)
	: _rep(), _default_value() {
	x.swap(*this);
    }
#endif

    /** @brief Destroy this hash table, freeing its memory. */
    ~HashTable() {
    }


    /** @brief Return the number of elements in the hash table. */
    inline size_type size() const {
	return _rep.size();
    }

    /** @brief Return true iff size() == 0. */
    inline bool empty() const {
	return _rep.empty();
    }

    /** @brief Return the number of buckets in the hash table. */
    inline size_type bucket_count() const {
	return _rep.bucket_count();
    }

    /** @brief Return the number of elements in bucket @a n.
     * @param n bucket number, >= 0 and < bucket_count() */
    inline size_type bucket_size(size_type n) const {
	return _rep.bucket_size(n);
    }

    /** @brief Return the hash table's default value.
     *
     * The default value is returned by operator[]() when a key does not
     * exist. */
    inline const mapped_type &default_value() const {
	return _default_value;
    }


    typedef HashTable_const_iterator<value_type> const_iterator;
    typedef HashTable_iterator<value_type> iterator;

    /** @brief Return an iterator for the first element in the table.
     *
     * @note HashTable iterators return elements in undefined order. */
    inline iterator begin() {
	return _rep.begin();
    }
    /** @overload */
    inline const_iterator begin() const {
	return _rep.begin();
    }

    /** @brief Return an iterator for the end of the table.
     * @invariant end().live() == false */
    inline iterator end() {
	return _rep.end();
    }
    /** @overload */
    inline const_iterator end() const {
	return _rep.end();
    }


    /** @brief Return 1 if an element with key @a key exists, 0 otherwise. */
    inline size_type count(key_const_reference key) const {
        return _rep.count(key);
    }

    /** @brief Return an iterator for the element with key @a key, if any.
     *
     * Returns end() if no such element exists. */
    inline const_iterator find(key_const_reference key) const {
	return _rep.find(key);
    }
    /** @overload */
    inline iterator find(key_const_reference key) {
	return _rep.find(key);
    }

    /** @brief Return an iterator for the element with key @a key, if any.
     *
     * Like find(), but additionally moves the found element to the head of
     * its bucket, possibly speeding up future lookups. */
    inline iterator find_prefer(key_const_reference key) {
	return _rep.find_prefer(key);
    }


    /** @brief Return the value for @a key.
     *
     * If no element for @a key currently exists (find(@a key) == end()),
     * returns default_value(). */
    const mapped_type &get(key_const_reference key) const {
	if (const_iterator i = find(key))
	    return i.value();
	else
	    return _default_value;
    }

    /** @brief Return a pointer to the value for @a key.
     *
     * If no element for @a key currently exists (find(@a key) == end()),
     * returns null. */
    mapped_type *get_pointer(key_const_reference key) {
	if (iterator i = find(key))
	    return &i.value();
	else
	    return 0;
    }
    /** @overload */
    const mapped_type *get_pointer(key_const_reference key) const {
	if (const_iterator i = find(key))
	    return &i.value();
	else
	    return 0;
    }

    /** @brief Return the value for @a key.
     *
     * If no element for @a key currently exists (find(@a key) == end()),
     * returns default_value().
     *
     * @warning The overloaded operator[] on non-const hash tables may add an
     * element to the table.  If you don't want to add an element, either
     * access operator[] through a const hash table, or use get().  */
    const mapped_type &operator[](key_const_reference key) const {
	if (const_iterator i = find(key))
	    return i.value();
	else
	    return _default_value;
    }

    /** @brief Return a reference to the value for @a key.
     *
     * The caller can assign the reference to change the value.  If no element
     * for @a key currently exists (find(@a key) == end()), adds a new element
     * with default_value() and returns a reference to that value.
     *
     * @note Inserting an element into a HashTable invalidates all existing
     * iterators. */
#if CLICK_HASHMAP_UPGRADE_WARNINGS
    inline mapped_type &operator[](key_const_reference key) CLICK_DEPRECATED;
#else
    inline mapped_type &operator[](key_const_reference key);
#endif


    /** @brief Ensure an element with key @a key and return its iterator.
     *
     * If an element with @a key already exists in the table, then find(@a
     * key) and find_insert(@a key) are equivalent.  Otherwise, find_insert
     * adds a new element with key @a key and value default_value() to the
     * table and returns its iterator.
     *
     * @note Inserting an element into a HashTable invalidates all existing
     * iterators. */
    inline iterator find_insert(key_const_reference key) {
	return _rep.find_insert(value_type(key, _default_value));
    }

    /** @brief Ensure an element for key @a key and return its iterator.
     *
     * If an element with @a key already exists in the table, then find(@a
     * key) and find_insert(@a value) are equivalent.  Otherwise,
     * find_insert(@a key, @a value) adds a new element with key @a key and
     * value @a value to the table and returns its iterator.
     *
     * @note Inserting an element into a HashTable invalidates all existing
     * iterators. */
    inline iterator find_insert(key_const_reference key,
                                const mapped_type &value) {
	return _rep.find_insert(value_type(key, value));
    }


    /** @brief Set the mapping for @a key to @a value.
     *
     * If an element for @a key already exists in the table, then its value is
     * assigned to @a value and the function returns false.  Otherwise, a new
     * element mapping @a key to @a value is added and the function returns
     * true.
     *
     * The behavior is basically the same as "(*this)[@a key] = @a value".
     * (The difference is that if (*this)[@a key] is not already in the hash
     * table, the new @a value is constructed rather than assigned.)
     *
     * @note Inserting an element into a HashTable invalidates all existing
     * iterators. */
    bool set(key_const_reference key, const mapped_type &value);

    /** @brief Set the mapping for @a key to @a value.
     *
     * This is a deprecated synonym for set().
     *
     * @deprecated Use set(). */
    bool replace(key_const_reference key, const mapped_type &value) CLICK_DEPRECATED;

    /** @brief Remove the element indicated by @a it.
     * @return A valid iterator pointing at the next element remaining, or
     * end() if no such element exists. */
    iterator erase(const iterator &it) {
	return _rep.erase(it);
    }

    /** @brief Remove any element with @a key.
     *
     * Returns the number of elements removed, which is always 0 or 1. */
    size_type erase(key_const_reference key) {
	return _rep.erase(key);
    }

    /** @brief Remove all elements.
     * @post size() == 0 */
    void clear() {
	_rep.clear();
    }


    /** @brief Swap the contents of this hash table and @a x. */
    void swap(HashTable<K, V> &x) {
	_rep.swap(x._rep);
	click_swap(x._default_value, _default_value);
    }


    /** @brief Rehash the table, ensuring it contains at least @a n buckets.
     *
     * All existing iterators are invalidated.  If @a n < bucket_count(), this
     * function may make the hash table slower. */
    void rehash(size_type nb) {
	_rep.rehash(nb);
    }


    /** @brief Assign this hash table's contents to a copy of @a x. */
    HashTable<K, V> &operator=(const HashTable<K, V> &x) {
	_rep = x._rep;
	_default_value = x._default_value;
	return *this;
    }

#if HAVE_CXX_RVALUE_REFERENCES
    /** @overload */
    HashTable<K, V> &operator=(HashTable<K, V> &&x) {
	x.swap(*this);
	return *this;
    }
#endif

  private:

    rep_type _rep;
    V _default_value;

};



template <typename T>
HashTable<T>::~HashTable()
{
    for (typename rep_type::iterator it = _rep.begin(); it; ) {
	elt *e = _rep.erase(it);
	e->v.~T();
	_alloc.deallocate(e);
    }
}

template <typename T>
inline typename HashTable<T>::const_iterator HashTable<T>::begin() const
{
    return const_iterator(_rep.begin());
}

template <typename T>
inline typename HashTable<T>::iterator HashTable<T>::begin()
{
    return iterator(_rep.begin());
}

template <typename T>
inline typename HashTable<T>::const_iterator HashTable<T>::end() const
{
    return const_iterator(_rep.end());
}

template <typename T>
inline typename HashTable<T>::iterator HashTable<T>::end()
{
    return iterator(_rep.end());
}

template <typename T>
inline typename HashTable<T>::size_type HashTable<T>::count(key_const_reference key) const
{
    return _rep.count(key);
}

template <typename T>
inline HashTable_const_iterator<T> HashTable<T>::find(key_const_reference key) const
{
    return HashTable_const_iterator<T>(_rep.find(key));
}

template <typename T>
inline HashTable_iterator<T> HashTable<T>::find(key_const_reference key)
{
    return HashTable_iterator<T>(_rep.find(key));
}

template <typename T>
inline HashTable_iterator<T> HashTable<T>::find_prefer(key_const_reference key)
{
    return HashTable_iterator<T>(_rep.find_prefer(key));
}

template <typename T>
HashTable_iterator<T> HashTable<T>::find_insert(key_const_reference key)
{
    typename rep_type::iterator i = _rep.find(key);
    if (!i)
	if (elt *e = reinterpret_cast<elt *>(_alloc.allocate())) {
	    new(reinterpret_cast<void *>(&e->v)) T(key);
	    _rep.set(i, e, true);
	}
    return i;
}

template <typename T>
typename HashTable<T>::value_type &
HashTable<T>::operator[](key_const_reference key)
{
    return *find_insert(key);
}

template <typename T>
HashTable_iterator<T> HashTable<T>::find_insert(const value_type &v)
{
    typename rep_type::iterator i = _rep.find(hashkey(v));
    if (!i)
	if (elt *e = reinterpret_cast<elt *>(_alloc.allocate())) {
	    new(reinterpret_cast<void *>(&e->v)) T(v);
	    _rep.set(i, e, true);
	}
    return i;
}

template <typename T>
bool HashTable<T>::set(const value_type &value)
{
    typename rep_type::iterator i = _rep.find(hashkey(value));
    if (i)
	i->v = value;
    else if (elt *e = reinterpret_cast<elt *>(_alloc.allocate())) {
	new(reinterpret_cast<void *>(&e->v)) T(value);
	_rep.set(i, e, true);
	return true;
    }
    return false;
}

template <typename K, typename V>
bool HashTable<K, V>::set(const key_type &key, const mapped_type &value)
{
    typename rep_type::rep_type::iterator i = _rep._rep.find(key);
    if (i)
	i->v.second = value;
    else if (typename rep_type::elt *e = reinterpret_cast<typename rep_type::elt *>(_rep._alloc.allocate())) {
	new(reinterpret_cast<void *>(&e->v)) value_type(key, value);
	_rep._rep.set(i, e, true);
	return true;
    }
    return false;
}

template <typename T>
typename HashTable<T>::iterator HashTable<T>::erase(const iterator &it)
{
    iterator itx(it);
    if (elt *e = _rep.erase(reinterpret_cast<typename rep_type::iterator &>(itx._rep))) {
	e->v.~T();
	_alloc.deallocate(e);
    }
    return itx;
}

template <typename T>
typename HashTable<T>::size_type HashTable<T>::erase(const key_type &key)
{
    if (elt *e = _rep.erase(key)) {
	e->v.~T();
	_alloc.deallocate(e);
	return 1;
    } else
	return 0;
}

template <typename T>
void HashTable<T>::clone_elements(const HashTable<T> &o)
  // requires that 'this' is empty and has the same number of buckets as 'o'
{
    size_type b = (size_type) -1;
    typename rep_type::iterator j = _rep.end();
    for (typename rep_type::const_iterator i = o._rep.begin(); i; ++i) {
	if (b != i.bucket())
	    j = _rep.begin((b = i.bucket()));
	if (elt *e = reinterpret_cast<elt *>(_alloc.allocate())) {
	    new(reinterpret_cast<void *>(&e->v)) T(i->v);
	    _rep.insert_at(j, e);
	}
    }
}

template <typename T>
void HashTable<T>::copy_elements(const HashTable<T> &o)
{
    for (typename rep_type::const_iterator i = o._rep.begin(); i; ++i)
	find_insert(i->v);
}

template <typename T>
HashTable<T> &HashTable<T>::operator=(const HashTable<T> &o)
{
    if (&o != this) {
	clear();
	if (_rep.bucket_count() < o._rep.bucket_count())
	    _rep.rehash(o._rep.bucket_count());
	copy_elements(o);
    }
    return *this;
}

template <typename T>
void HashTable<T>::clear()
{
    for (typename rep_type::iterator it = _rep.begin(); it; ) {
	elt *e = _rep.erase(it);
	e->v.~T();
	_alloc.deallocate(e);
    }
}

template <typename T>
void HashTable<T>::swap(HashTable<T> &o)
{
    _rep.swap(o._rep);
    _alloc.swap(o._alloc);
}

template <typename K, typename V>
inline typename HashTable<K, V>::mapped_type &HashTable<K, V>::operator[](const key_type &key)
{
    return find_insert(key).value();
}

template <typename K, typename V>
bool HashTable<K, V>::replace(const key_type &key, const mapped_type &value)
{
    return set(key, value);
}


/** @brief Compare two HashTable iterators for equality. */
template <typename T>
inline bool operator==(const HashTable_const_iterator<T> &a, const HashTable_const_iterator<T> &b)
{
    return a.get() == b.get();
}

/** @brief Compare two HashTable iterators for inequality. */
template <typename T>
inline bool operator!=(const HashTable_const_iterator<T> &a, const HashTable_const_iterator<T> &b)
{
    return a.get() != b.get();
}


template <typename K, typename V>
inline void click_swap(HashTable<K, V> &a, HashTable<K, V> &b)
{
    a.swap(b);
}

template <typename K, typename V>
inline void assign_consume(HashTable<K, V> &a, HashTable<K, V> &b)
{
    a.swap(b);
}

template <typename K, typename V>
inline void clear_by_swap(HashTable<K, V> &x)
{
    // specialization avoids losing x's default value
    HashTable<K, V> tmp(x.default_value());
    x.swap(tmp);
}

CLICK_ENDDECLS
#endif
