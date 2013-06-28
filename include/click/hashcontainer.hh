#ifndef CLICK_HASHCONTAINER_HH
#define CLICK_HASHCONTAINER_HH
#include <click/glue.hh>
#include <click/hashcode.hh>
#if CLICK_DEBUG_HASHMAP
# define click_hash_assert(x) assert(x)
#else
# define click_hash_assert(x)
#endif
CLICK_DECLS

template <typename T> class HashContainer_adapter;
template <typename T, typename A = HashContainer_adapter<T> > class HashContainer_const_iterator;
template <typename T, typename A = HashContainer_adapter<T> > class HashContainer_iterator;
template <typename T, typename A = HashContainer_adapter<T> > class HashContainer;

/** @cond never */
template <typename T, typename A>
class HashContainer_rep : public A {
    T **buckets;
    size_t nbuckets;
    size_t size;
    mutable size_t first_bucket;
    friend class HashContainer<T, A>;
    friend class HashContainer_const_iterator<T, A>;
    friend class HashContainer_iterator<T, A>;
};
/** @endcond */

template <typename T>
class HashContainer_adapter { public:
    typedef typename T::key_type key_type;
    typedef typename T::key_const_reference key_const_reference;
    static T *&hashnext(T *e) {
	return e->_hashnext;
    }
    static key_const_reference hashkey(const T *e) {
	return e->hashkey();
    }
    static bool hashkeyeq(const key_type &a, const key_type &b) {
	return a == b;
    }
};

/** @class HashContainer
  @brief Intrusive hash table template.

  The HashContainer template implements a hash table or associative array
  suitable for use in the kernel or at user level.

  HashContainer is <em>intrusive.</em> This means it does not manage its
  contents' memory.  While non-intrusive containers are more common in the
  STL, intrusive containers make it simple to store objects in more than one
  container at a time.

  Unlike many hash tables HashContainer does not automatically grow itself to
  maintain good lookup performance.  Its users are expected to call rehash()
  when appropriate.  See unbalanced().

  With the default adapter type (A), the template type T must:

  <ul>
  <li>Define a "key_type" type that supports equality.</li>
  <li>Define a "key_const_reference" type, usually the same as "key_type."</li>
  <li>Contain a member "T *_hashnext" accessible to HashContainer_adapter<T>.</li>
  <li>Define a "hashkey()" member function that returns the relevant hash key.
  This function must have return type "key_const_reference."</li>
  </ul>

  These requirements can be changed by supplying a different A, or adapter,
  type.

  HashContainer can store multiple elements with the same key, although this
  is not the normal use.  An element stored in a HashContainer should not
  modify its key.

  HashContainer is used to implement Click's HashTable template.
*/
template <typename T, typename A>
class HashContainer { public:

    /** @brief Key type. */
    typedef typename A::key_type key_type;

    /** @brief Value type.
     *
     * Must meet the HashContainer requirements defined by type A. */
    typedef T value_type;

    /** @brief Type of sizes. */
    typedef size_t size_type;

    enum {
#if CLICK_LINUXMODULE
	max_bucket_count = 4194303,
#else
	max_bucket_count = (size_t) -1,
#endif
	initial_bucket_count = 63
    };

    /** @brief Construct an empty HashContainer. */
    HashContainer();

    /** @brief Construct an empty HashContainer with at least @a n buckets. */
    explicit HashContainer(size_type n);

    /** @brief Destroy the HashContainer. */
    ~HashContainer();


    /** @brief Return the number of elements stored. */
    inline size_type size() const {
	return _rep.size;
    }

    /** @brief Return true iff size() == 0. */
    inline bool empty() const {
	return _rep.size == 0;
    }

    /** @brief Return the number of buckets. */
    inline size_type bucket_count() const {
	return _rep.nbuckets;
    }

    /** @brief Return the number of elements in bucket @a n. */
    inline size_type bucket_size(size_type n) const {
	click_hash_assert(n < _rep.nbuckets);
	size_type s = 0;
	for (T *element = _rep.buckets[n]; element; element = _rep.hashnext(element))
	    ++s;
	return s;
    }

    /** @brief Return the bucket number containing elements with @a key. */
    size_type bucket(const key_type &key) const;

    /** @brief Return true if this HashContainer should be rebalanced. */
    inline bool unbalanced() const {
	return _rep.size > 2 * _rep.nbuckets && _rep.nbuckets < max_bucket_count;
    }

    typedef HashContainer_const_iterator<T, A> const_iterator;
    typedef HashContainer_iterator<T, A> iterator;

    /** @brief Return an iterator for the first element in the container.
     *
     * @note HashContainer iterators return elements in random order. */
    inline iterator begin();
    /** @overload */
    inline const_iterator begin() const;

    /** @brief Return an iterator for the end of the container.
     * @invariant end().live() == false */
    inline iterator end();
    /** @overload */
    inline const_iterator end() const;

    /** @brief Return an iterator for the first element in bucket @a n. */
    inline iterator begin(size_type n);
    /** @overload */
    inline const_iterator begin(size_type n) const;

    /** @brief Test if an element with key @a key exists in the table. */
    inline bool contains(const key_type& key) const;
    /** @brief Return the number of elements with key @a key in the table. */
    inline size_type count(const key_type& key) const;

    /** @brief Return an iterator for an element with @a key, if any.
     *
     * If no element with @a key exists in the table, find() returns an
     * iterator that compares equal to end().  However, this iterator is
     * special, and can also be used to efficiently insert an element with key
     * @a key.  In particular, the return value of find() always has
     * can_insert(), and can thus be passed to insert_at() or set().  (It
     * will insert elements at the head of the relevant bucket.) */
    inline iterator find(const key_type &key);
    /** @overload */
    inline const_iterator find(const key_type &key) const;

    /** @brief Return an iterator for an element with key @a key, if any.
     *
     * Like find(), but additionally moves any found element to the head of
     * its bucket, possibly speeding up future lookups. */
    inline iterator find_prefer(const key_type &key);

    /** @brief Return an element for @a key, if any.
     *
     * Returns null if no element for @a key currently exists.  Equivalent
     * to find(key).get(). */
    inline T *get(const key_type &key) const;

    /** @brief Insert an element at position @a it.
     * @param it iterator
     * @param element element
     * @pre @a it.can_insert()
     * @pre @a it.bucket() == bucket(@a element->hashkey())
     * @pre @a element != NULL
     * @pre @a element is not already in the HashContainer
     *
     * Inserts @a element at the position in the hash table indicated by @a
     * it.  For instance, if @a it == begin(@em n) for some bucket number @em
     * n, then @a element becomes the first element in bucket @em n.  Other
     * elements in the bucket, if any, are chained along.
     *
     * On return, @a it is updated to point immediately after @a element.
     * If @a it was not live before, then it will not be live after.
     *
     * @note HashContainer never automatically rehashes itself, so element
     * insertion leaves any existing iterators valid.  For best performance,
     * however, users must call balance() to resize the container when it
     * becomes unbalanced(). */
    inline void insert_at(iterator &it, T *element);

    /** @brief Replace the element at position @a it with @a element.
     * @param it iterator
     * @param element element (can be null)
     * @param balance whether to balance the hash table
     * @return the previous value of it.get()
     * @pre @a it.can_insert()
     * @pre @a it.bucket() == bucket(@a element->hashkey())
     * @pre @a element is not already in the HashContainer
     *
     * Replaces the element pointed to by @a it with @a element, and returns
     * the former element.  If @a element is null the former element is
     * removed.  If there is no former element then @a element is inserted.
     * When inserting an element with @a balance true, set() may rebalance the
     * hash table.
     *
     * As a side effect, @a it is advanced to point at the newly inserted @a
     * element.  If @a element is null, then @a it is advanced to point at the
     * next element as by ++@a it. */
    T *set(iterator &it, T *element, bool balance = false);

    /** @brief Replace the element with @a element->hashkey() with @a element.
     * @param element element
     * @return the previous value of find(@a element->hashkey()).get()
     * @pre @a element is not already in the HashContainer
     *
     * Finds an element with the same hashkey as @a element, removes it from
     * the HashContainer, and replaces it with @a element.  If there is no
     * former element then @a element is inserted. */
    inline T *set(T *element);

    /** @brief Remove the element at position @a it.
     * @param it iterator
     * @return the previous value of it.get()
     *
     * As a side effect, @a it is advanced to the next element as by ++@a it. */
    inline T *erase(iterator &it);

    /** @brief Remove an element with hashkey @a key.
     * @return the element removed, if any
     *
     * Roughly equivalent to erase(find(key)). */
    inline T *erase(const key_type &key);

    /** @brief Removes all elements from the container.
     * @post size() == 0 */
    void clear();

    /** @brief Swaps the contents of *this and @a x. */
    inline void swap(HashContainer<T, A> &x);

    /** @brief Rehash the table, ensuring it contains at least @a n buckets.
     *
     * If @a n < bucket_count(), this function may make the hash table
     * slower.
     *
     * @note Rehashing invalidates all existing iterators. */
    void rehash(size_type n);

    /** @brief Rehash the table if it is unbalanced.
     *
     * @note Rehashing invalidates all existing iterators. */
    inline void balance() {
	if (unbalanced())
	    rehash(bucket_count() + 1);
    }

  private:

    HashContainer_rep<T, A> _rep;

    HashContainer(const HashContainer<T, A> &);
    HashContainer<T, A> &operator=(const HashContainer<T, A> &);

    friend class HashContainer_iterator<T, A>;
    friend class HashContainer_const_iterator<T, A>;

};

/** @class HashContainer_const_iterator
 * @brief The const_iterator type for HashContainer. */
template <typename T, typename A>
class HashContainer_const_iterator { public:

    typedef typename HashContainer<T, A>::size_type size_type;

    /** @brief Construct an uninitialized iterator. */
    HashContainer_const_iterator() {
    }

    /** @brief Return a pointer to the element, null if *this == end(). */
    T *get() const {
	return _element;
    }

    /** @brief Return a pointer to the element, null if *this == end(). */
    T *operator->() const {
	return _element;
    }

    /** @brief Return a reference to the element.
     * @pre *this != end() */
    T &operator*() const {
	return *_element;
    }

    /** @brief Return true iff *this != end(). */
    inline bool live() const {
	return _element;
    }

    typedef T *(HashContainer_const_iterator::*unspecified_bool_type)() const;
    /** @brief Return true iff *this != end(). */
    inline operator unspecified_bool_type() const {
	return _element ? &HashContainer_const_iterator::get : 0;
    }

    /** @brief Return the corresponding HashContainer. */
    const HashContainer<T, A> *hashcontainer() const {
	return _hc;
    }

    /** @brief Return the bucket number this iterator is in. */
    size_type bucket() const {
	return _bucket;
    }

    /** @brief Advance this iterator to the next element. */
    void operator++() {
	if (_element && _hc->_rep.hashnext(_element)) {
	    _pprev = &_hc->_rep.hashnext(_element);
	    _element = *_pprev;
	} else if (_bucket != _hc->_rep.nbuckets) {
	    for (++_bucket; _bucket != _hc->_rep.nbuckets; ++_bucket)
		if (*(_pprev = &_hc->_rep.buckets[_bucket])) {
		    _element = *_pprev;
		    return;
		}
	    _element = 0;
	}
    }

    /** @brief Advance this iterator to the next element. */
    void operator++(int) {
	++*this;
    }

  private:

    T *_element;
    T **_pprev;
    size_type _bucket;
    const HashContainer<T, A> *_hc;

    inline HashContainer_const_iterator(const HashContainer<T, A> *hc)
	: _hc(hc) {
	_bucket = hc->_rep.first_bucket;
	_pprev = &hc->_rep.buckets[_bucket];
	if (unlikely(_bucket == hc->_rep.nbuckets))
	    _element = 0;
	else if (!(_element = *_pprev)) {
	    (*this)++;
	    hc->_rep.first_bucket = _bucket;
	}
    }

    inline HashContainer_const_iterator(const HashContainer<T, A> *hc, size_type b, T **pprev, T *element)
	: _element(element), _pprev(pprev), _bucket(b), _hc(hc) {
	click_hash_assert((!_pprev && !_element) || *_pprev == _element);
    }

    friend class HashContainer<T, A>;
    friend class HashContainer_iterator<T, A>;

};

/** @class HashContainer_iterator
  @brief The iterator type for HashContainer. */
template <typename T, typename A>
class HashContainer_iterator : public HashContainer_const_iterator<T, A> { public:

    typedef HashContainer_const_iterator<T, A> inherited;

    /** @brief Construct an uninitialized iterator. */
    HashContainer_iterator() {
    }

    /** @brief Return true iff elements can be inserted here.
     *
     * Specifically, returns true iff this iterator is valid to pass to
     * HashContainer::insert_at() or HashContainer::set().  All live()
     * iterators can_insert(), but some !live() iterators can_insert() as
     * well. */
    bool can_insert() const {
	return this->_bucket < this->_hc->bucket_count();
    }

    /** @brief Return the corresponding HashContainer. */
    HashContainer<T, A> *hashcontainer() const {
	return const_cast<HashContainer<T, A> *>(this->_hc);
    }

  private:

    inline HashContainer_iterator(HashContainer<T, A> *hc)
	: inherited(hc) {
    }

    inline HashContainer_iterator(HashContainer<T, A> *hc, typename inherited::size_type b, T **pprev, T *element)
	: inherited(hc, b, pprev, element) {
    }

    friend class HashContainer<T, A>;

};

template <typename T, typename A>
HashContainer<T, A>::HashContainer()
{
    _rep.size = 0;
    _rep.nbuckets = initial_bucket_count;
    _rep.buckets = (T **) CLICK_LALLOC(sizeof(T *) * _rep.nbuckets);
    _rep.first_bucket = _rep.nbuckets;
    for (size_type b = 0; b < _rep.nbuckets; ++b)
	_rep.buckets[b] = 0;
}

template <typename T, typename A>
HashContainer<T, A>::HashContainer(size_type nb)
{
    size_type b = 1;
    while (b < nb && b < max_bucket_count)
	b = ((b + 1) << 1) - 1;
    _rep.size = 0;
    _rep.nbuckets = b;
    _rep.buckets = (T **) CLICK_LALLOC(sizeof(T *) * _rep.nbuckets);
    _rep.first_bucket = _rep.nbuckets;
    for (b = 0; b < _rep.nbuckets; ++b)
	_rep.buckets[b] = 0;
}

template <typename T, typename A>
HashContainer<T, A>::~HashContainer()
{
    CLICK_LFREE(_rep.buckets, sizeof(T *) * _rep.nbuckets);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::size_type
HashContainer<T, A>::bucket(const key_type &key) const
{
    return ((size_type) hashcode(key)) % _rep.nbuckets;
}

template <typename T, typename A>
inline typename HashContainer<T, A>::const_iterator
HashContainer<T, A>::begin() const
{
    return const_iterator(this);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::iterator
HashContainer<T, A>::begin()
{
    return iterator(this);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::const_iterator
HashContainer<T, A>::end() const
{
    return const_iterator(this, -1, 0, 0);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::iterator
HashContainer<T, A>::end()
{
    return iterator(this, -1, 0, 0);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::iterator
HashContainer<T, A>::begin(size_type b)
{
    click_hash_assert(b < _rep.nbuckets);
    return iterator(this, b, &_rep.buckets[b], _rep.buckets[b]);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::const_iterator
HashContainer<T, A>::begin(size_type b) const
{
    click_hash_assert(b < _rep.nbuckets);
    return const_iterator(this, b, &_rep.buckets[b], _rep.buckets[b]);
}

template <typename T, typename A>
inline bool HashContainer<T, A>::contains(const key_type& key) const
{
    size_type b = bucket(key);
    T **pprev;
    for (pprev = &_rep.buckets[b]; *pprev; pprev = &_rep.hashnext(*pprev))
	if (_rep.hashkeyeq(_rep.hashkey(*pprev), key))
            return true;
    return false;
}

template <typename T, typename A>
inline typename HashContainer<T, A>::size_type
HashContainer<T, A>::count(const key_type& key) const
{
    size_type b = bucket(key), c = 0;
    T **pprev;
    for (pprev = &_rep.buckets[b]; *pprev; pprev = &_rep.hashnext(*pprev))
	c += _rep.hashkeyeq(_rep.hashkey(*pprev), key);
    return c;
}

template <typename T, typename A>
inline typename HashContainer<T, A>::iterator
HashContainer<T, A>::find(const key_type &key)
{
    size_type b = bucket(key);
    T **pprev;
    for (pprev = &_rep.buckets[b]; *pprev; pprev = &_rep.hashnext(*pprev))
	if (_rep.hashkeyeq(_rep.hashkey(*pprev), key))
	    return iterator(this, b, pprev, *pprev);
    return iterator(this, b, &_rep.buckets[b], 0);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::const_iterator
HashContainer<T, A>::find(const key_type &key) const
{
    return const_cast<HashContainer<T, A> *>(this)->find(key);
}

template <typename T, typename A>
inline typename HashContainer<T, A>::iterator
HashContainer<T, A>::find_prefer(const key_type &key)
{
    size_type b = bucket(key);
    T **pprev;
    for (pprev = &_rep.buckets[b]; *pprev; pprev = &_rep.hashnext(*pprev))
	if (_rep.hashkeyeq(_rep.hashkey(*pprev), key)) {
	    T *element = *pprev;
	    *pprev = _rep.hashnext(element);
	    _rep.hashnext(element) = _rep.buckets[b];
	    _rep.buckets[b] = element;
	    return iterator(this, b, &_rep.buckets[b], element);
	}
    return iterator(this, b, &_rep.buckets[b], 0);
}

template <typename T, typename A>
inline T *HashContainer<T, A>::get(const key_type &key) const
{
    return find(key).get();
}

template <typename T, typename A>
T *HashContainer<T, A>::set(iterator &it, T *element, bool balance)
{
    click_hash_assert(it._hc == this && it._bucket < _rep.nbuckets);
    click_hash_assert(bucket(_rep.hashkey(element)) == it._bucket);
    click_hash_assert(!it._element || _rep.hashkeyeq(_rep.hashkey(element), _rep.hashkey(it._element)));
    T *old = it.get();
    if (unlikely(old == element))
	return old;
    if (!element) {
	--_rep.size;
	if (!(*it._pprev = it._element = _rep.hashnext(old)))
	    ++it;
	return old;
    }
    if (old)
	_rep.hashnext(element) = _rep.hashnext(old);
    else {
	++_rep.size;
	if (unlikely(unbalanced()) && balance) {
	    rehash(bucket_count() + 1);
	    it._bucket = bucket(_rep.hashkey(element));
	    it._pprev = &_rep.buckets[it._bucket];
	}
	if (!(_rep.hashnext(element) = *it._pprev))
	    _rep.first_bucket = 0;
    }
    *it._pprev = it._element = element;
    return old;
}

template <typename T, typename A>
inline void HashContainer<T, A>::insert_at(iterator &it, T *element)
{
    click_hash_assert(it._hc == this && it._bucket < _rep.nbuckets);
    click_hash_assert(bucket(_rep.hashkey(element)) == it._bucket);
    ++_rep.size;
    if (!(_rep.hashnext(element) = *it._pprev))
	_rep.first_bucket = 0;
    *it._pprev = element;
    it._pprev = &_rep.hashnext(element);
}

template <typename T, typename A>
inline T *HashContainer<T, A>::set(T *element)
{
    iterator it = find(_rep.hashkey(element));
    return set(it, element);
}

template <typename T, typename A>
inline T *HashContainer<T, A>::erase(iterator &it)
{
    click_hash_assert(it._hc == this);
    return set(it, 0);
}

template <typename T, typename A>
inline T *HashContainer<T, A>::erase(const key_type &key)
{
    iterator it = find(key);
    return set(it, 0);
}

template <typename T, typename A>
inline void HashContainer<T, A>::clear()
{
    for (size_type b = 0; b < _rep.nbuckets; ++b)
	_rep.buckets[b] = 0;
    _rep.size = 0;
}

template <typename T, typename A>
inline void HashContainer<T, A>::swap(HashContainer<T, A> &o)
{
    HashContainer_rep<T, A> rep(_rep);
    _rep = o._rep;
    o._rep = rep;
}

template <typename T, typename A>
void HashContainer<T, A>::rehash(size_type n)
{
    size_type new_nbuckets = 1;
    while (new_nbuckets < n && new_nbuckets < max_bucket_count)
	new_nbuckets = ((new_nbuckets + 1) << 1) - 1;
    click_hash_assert(new_nbuckets > 0 && new_nbuckets <= max_bucket_count);
    if (_rep.nbuckets == new_nbuckets)
	return;

    T **new_buckets = (T **) CLICK_LALLOC(sizeof(T *) * new_nbuckets);
    for (size_type b = 0; b < new_nbuckets; ++b)
	new_buckets[b] = 0;

    size_type old_nbuckets = _rep.nbuckets;
    T **old_buckets = _rep.buckets;
    _rep.nbuckets = new_nbuckets;
    _rep.buckets = new_buckets;
    _rep.first_bucket = 0;

    for (size_t b = 0; b < old_nbuckets; b++)
	for (T *element = old_buckets[b]; element; ) {
	    T *next = _rep.hashnext(element);
	    size_type new_b = bucket(_rep.hashkey(element));
	    _rep.hashnext(element) = new_buckets[new_b];
	    new_buckets[new_b] = element;
	    element = next;
	}

    CLICK_LFREE(old_buckets, sizeof(T *) * old_nbuckets);
}

template <typename T, typename A>
inline bool
operator==(const HashContainer_const_iterator<T, A> &a, const HashContainer_const_iterator<T, A> &b)
{
    click_hash_assert(a.hashcontainer() == b.hashcontainer());
    return a.get() == b.get();
}

template <typename T, typename A>
inline bool
operator!=(const HashContainer_const_iterator<T, A> &a, const HashContainer_const_iterator<T, A> &b)
{
    click_hash_assert(a.hashcontainer() == b.hashcontainer());
    return a.get() != b.get();
}

CLICK_ENDDECLS
#endif
