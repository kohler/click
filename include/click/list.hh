#ifndef CLICK_LIST_HH
#define CLICK_LIST_HH 1
/*
 * list.hh -- List template
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
CLICK_DECLS
#define LIST_HEAD_MARKER 0 /* ((T *) 1) */

/** @file <click/list.hh>
 * @brief Click's doubly-linked list container template.
 */

template <typename T> class List_member;
template <typename T, List_member<T> T::*member> class List;

/** @class List
  @brief Doubly-linked list template.

  The List template, and its helper template List_member, implement a generic
  doubly-linked list.  The list is <em>intrusive</em> in that the container
  does not manage space for its contents.  The user provides space for
  contained elements, and must delete elements when they are no longer needed.
  (This is unlike Vector or HashTable, which manage space for their contents.)
  The main advantage of intrusive containers is that a single element can be
  on multiple lists.

  Here's an example linked list of integers built using List and List_member.

  @code
  #include <click/list.hh>

  struct intlist_node {
      int value;
      List_member<intlist_node> link;
      intlist_node(int v)
          : value(v) {
      }
  };

  typedef List<intlist_node, &intlist_node::link> intlist;

  void make_intlist(intlist &l, int begin, int end, int step) {
      for (int i = begin; i < end; i += step)
          l.push_back(new intlist_node(i));
      // Note that l does not manage its contents' memory!
      // Whoever destroys l should first delete its contents,
      // for example by calling trash_intlist(l).
  }

  void print_intlist(const intlist &l) {
      size_t n = 0;
      for (intlist::const_iterator it = l.begin(); it != l.end(); ++it, ++n)
          click_chatter("#%ld: %d\n", (long) n, it->value);
  }

  void trash_intlist(intlist &l) {
      while (!l.empty()) {
          intlist_node *n = l.front();
	  l.pop_front();
	  delete n;
      }
  }

  template <typename T>
  void remove_every_other(T &list) {
      typename T::iterator it = list.begin();
      while (it != l.end()) {
          ++it;
	  if (it != l.end())
	      it = list.erase(it);
      }
  }
  @endcode
*/

/** @class List_member
  @brief Member of classes to be placed on a List.

  Any class type that will be placed on a List must have a publicly accessible
  List_member member.  This member is supplied as the second template argument
  to List.  List_member allows users to fetch the next-element and
  previous-element pointers, but all modifications must take place via List
  functions like List::push_back() and List::insert().  List_member has
  private copy constructor and default assignment operators.

  @sa List
*/
template <typename T>
class List_member { public:

    /** @brief Construct an isolated List_member. */
    List_member()
	: _next(), _prev() {
    }

    /** @brief Return the next element in the list. */
    T *next() {
	return _next;
    }
    /** @overload */
    const T *next() const {
	return _next;
    }

    /** @brief Return the previous element in the list. */
    T *prev() {
	return _prev != LIST_HEAD_MARKER ? _prev : 0;
    }
    /** @overload */
    const T *prev() const {
	return _prev != LIST_HEAD_MARKER ? _prev : 0;
    }

  private:

    T *_next;
    T *_prev;

    List_member(const List_member<T> &x);
    List_member<T> &operator=(const List_member<T> &x);
    template <typename X, List_member<X> X::*member> friend class List;

};

template <typename T, List_member<T> T::*member>
class List { public:

    typedef T *pointer;
    typedef const T *const_pointer;
    class const_iterator;
    class iterator;
    typedef size_t size_type;

    /** @brief Construct an empty list. */
    List()
	: _head(0), _tail(0) {
    }


    /** @brief Return an iterator for the first element in the list. */
    iterator begin() {
	return iterator(_head, this);
    }
    /** @overload */
    const_iterator begin() const {
	return const_iterator(_head, this);
    }
    /** @brief Return an iterator for the end of the list.
     * @invariant end().live() == false */
    iterator end() {
	return iterator(this);
    }
    /** @overload */
    const_iterator end() const {
	return const_iterator(this);
    }


    /** @brief Return true iff size() == 0.
     * @note Always O(1) time, whereas size() takes O(N) time. */
    bool empty() const {
	return _head == 0;
    }

    /** @brief Return the number of elements in the list.
     * @note Takes O(N) time, where N is the number of elements. */
    size_type size() const {
	size_type n = 0;
	for (T *x = _head; x; x = (x->*member)._next)
	    ++n;
	return n;
    }


    /** @brief Return the first element in the list.
     *
     * Returns a null pointer if the list is empty. */
    pointer front() {
	return _head;
    }
    /** @overload */
    const_pointer front() const {
	return _head;
    }
    /** @brief Return the last element in the list.
     *
     * Returns a null pointer if the list is empty. */
    pointer back() {
	return _tail;
    }
    /** @overload */
    const_pointer back() const {
	return _tail;
    }


    /** @brief Insert a new element at the beginning of the list.
     * @param x new element
     * @pre isolated(@a x) */
    void push_front(pointer x) {
	assert(x && isolated(x));
	if (((x->*member)._next = _head))
	    (_head->*member)._prev = x;
	else
	    _tail = x;
	_head = x;
	(_head->*member)._prev = LIST_HEAD_MARKER;
    }

    /** @brief Insert a new element at the end of the list.
     * @param x new element
     * @pre isolated(@a x) */
    void push_back(pointer x) {
	assert(x && !(x->*member)._next && !(x->*member)._prev);
	if (((x->*member)._prev = _tail))
	    (_tail->*member)._next = x;
	else {
	    _head = x;
	    (_head->*member)._prev = LIST_HEAD_MARKER;
	}
	_tail = x;
    }

    /** @brief Remove the element at the beginning of the list.
     * @pre !empty() */
    void pop_front() {
	assert(_head);
	pointer x = _head;
	if ((_head = (x->*member)._next) != LIST_HEAD_MARKER)
	    (_head->*member)._prev = LIST_HEAD_MARKER;
	else
	    _head = _tail = 0;
	(x->*member)._next = (x->*member)._prev = 0;
    }

    /** @brief Remove the element at the end of the list.
     * @pre !empty() */
    void pop_back() {
	assert(_head);
	pointer x = _tail;
	if ((_tail = (x->*member)._prev) != LIST_HEAD_MARKER)
	    (_tail->*member)._next = 0;
	else
	    _head = _tail = 0;
	(x->*member)._next = (x->*member)._prev = 0;
    }


    /** @brief Insert an element before @a pos.
     * @param pos position to insert (if null, insert at end of list)
     * @param x new element
     * @pre (!@a pos || contains(@a pos)) && isolated(@a x) */
    void insert(pointer pos, pointer x) {
	assert(x && isolated(x));
	T **pprev = (pos ? &(pos->*member)._prev : &_tail);
	if (((x->*member)._prev = *pprev) != LIST_HEAD_MARKER)
	    ((x->*member)._prev->*member)._next = x;
	else
	    _head = x;
	*pprev = x;
	(x->*member)._next = pos;
    }

    /** @brief Insert an element before @a it.
     * @param it position to insert
     * @param x new element
     * @return an iterator pointing to @a x
     * @pre isolated(@a x) */
    iterator insert(iterator it, pointer x) {
	insert(it.get(), x);
	return iterator(x);
    }

    /** @brief Insert the elements in [@a first, @a last) before @a it.
     * @param it position to insert
     * @param first iterator to beginning of insertion sequence
     * @param last iterator to end of insertion sequence
     * @pre isolated(@a x) for each @a x in [@a first, @a last) */
    template <typename InputIterator>
    void insert(iterator it, InputIterator first, InputIterator last) {
	while (first != last) {
	    insert(it, *first);
	    ++first;
	}
    }


    /** @brief Remove @a x from the list.
     * @param x element to remove
     * @pre contains(@a x) */
    void erase(pointer x) {
	assert(x);
	T *n = (x->*member)._next, *p = (x->*member)._prev;
	if (n)
	    (n->*member)._prev = p;
	else
	    _tail = (p != LIST_HEAD_MARKER ? p : 0);
	if (p != LIST_HEAD_MARKER)
	    (p->*member)._next = n;
	else
	    _head = n;
	(x->*member)._next = (x->*member)._prev = 0;
    }

    /** @brief Remove the element pointed to by @a it from the list.
     * @param it element to remove
     * @return iterator pointing to the element after the removed element
     * @pre @a it.live() */
    iterator erase(iterator it) {
	assert(it);
	iterator next = iterator((it.get()->*member)._next);
	erase(it.get());
	return next;
    }

    /** @brief Remove the elements in [@a first, @a last) from the list.
     * @param first iterator to beginning of removal subsequence
     * @param last iterator to end of removal subsequence
     * @return iterator pointing to the element after the removed subsequence */
    iterator erase(iterator first, iterator last) {
	while (first != last)
	    first = erase(first);
	return first;
    }

    /** @brief Remove all elements from the list.
     * @note Equivalent to erase(begin(), end()). */
    void clear() {
	while (T *x = _head) {
	    _head = (x->*member)._next;
	    (x->*member)._next = (x->*member)._prev = 0;
	}
	_tail = 0;
    }

    /** @brief Remove all elements from the list.
     *
     * Unlike clear(), this function does not erase() any of the elements of
     * this list: those elements' next() and prev() members remain
     * unchanged. */
    void __clear() {
	_head = _tail = 0;
    }


    /** @brief Exchange list contents with list @a x. */
    void swap(List<T, member> &x) {
	T *h = x._head, *t = x._tail;
	x._head = _head, x._tail = _tail;
	_head = h, _tail = t;
    }


    /** @brief Check if @a x is isolated.
     *
     * An isolated element is not a member of any list. */
    bool isolated(const_pointer x) {
	return !(x->*member)._next && !(x->*member)._prev && x != _head;
    }

    /** @brief Check if @a x is a member of this list. */
    bool contains(const_pointer x) const {
	if (!isolated(x))
	    for (const_pointer *it = _head; it; it = (it->*member).next())
		if (x == it)
		    return true;
	return false;
    }


    /** @class List::const_iterator
     * @brief Const iterator type for List. */
    class const_iterator { public:
	/** @brief Construct an invalid iterator. */
	const_iterator()
	    : _x(), _list() {
	}
	/** @brief Construct an iterator pointing at @a x. */
	const_iterator(const T *x)
	    : _x(const_cast<T *>(x)), _list() {
	}
	/** @brief Construct an end iterator for @a list. */
	const_iterator(const List<T, member> *list)
	    : _x(), _list(list) {
	}
	/** @brief Construct an iterator pointing at @a x in @a list. */
	const_iterator(const T *x, const List<T, member> *list)
	    : _x(const_cast<T *>(x)), _list(list) {
	}
	typedef bool (const_iterator::*unspecified_bool_type)() const;
	/** @brief Test if this iterator points to a valid list element. */
	operator unspecified_bool_type() const {
	    return _x != 0 ? &const_iterator::live : 0;
	}
	/** @brief Test if this iterator points to a valid list element. */
	bool live() const {
	    return _x != 0;
	}
	/** @brief Return the current list element or null. */
	const T *get() const {
	    return _x;
	}
	/** @brief Return the current list element or null. */
	const T *operator->() const {
	    return _x;
	}
	/** @brief Return the current list element. */
	const T &operator*() const {
	    return *_x;
	}
	/** @brief Advance this iterator to the next element. */
	void operator++() {
	    assert(_x);
	    _x = (_x->*member).next();
	}
	/** @brief Advance this iterator to the next element. */
	void operator++(int) {
	    ++*this;
	}
	/** @brief Advance this iterator to the previous element. */
	void operator--() {
	    assert(_x ? (bool) (_x->*member).prev() : _list && _list->back());
	    if (_x)
		_x = (_x->*member).prev();
	    else
		_x = const_cast<T *>(_list->back());
	}
	/** @brief Advance this iterator to the previous element. */
	void operator--(int) {
	    --*this;
	}
	/** @brief Move this iterator forward by @a x positions.
	 * @return reference to this iterator
	 * @note This function takes O(abs(@a x)) time. */
	const_iterator &operator+=(int x) {
	    for (; x > 0; --x)
		++*this;
	    for (; x < 0; ++x)
		--*this;
	    return *this;
	}
	/** @brief Move this iterator backward by @a x positions.
	 * @return reference to this iterator
	 * @note This function takes O(abs(@a x)) time. */
	const_iterator &operator-=(int x) {
	    for (; x > 0; --x)
		--*this;
	    for (; x < 0; ++x)
		++*this;
	    return *this;
	}
	/** @brief Return an iterator @a x positions ahead. */
	const_iterator operator+(int x) const {
	    const_iterator it(*this);
	    return it += x;
	}
	/** @brief Return an iterator @a x positions behind. */
	const_iterator operator-(int x) const {
	    const_iterator it(*this);
	    return it -= x;
	}
	/** @brief Test if this iterator equals @a x. */
	bool operator==(const_iterator x) const {
	    return _x == x._x;
	}
	/** @brief Test if this iterator does not equal @a x. */
	bool operator!=(const_iterator x) const {
	    return _x != x._x;
	}
      private:
	T *_x;
	const List<T, member> *_list;
	friend class iterator;
    };

    /** @class List::iterator
     * @brief Iterator type for List. */
    class iterator : public const_iterator { public:
	/** @brief Construct an invalid iterator. */
	iterator()
	    : const_iterator() {
	}
	/** @brief Construct an iterator pointing at @a x. */
	iterator(T *x)
	    : const_iterator(x) {
	}
	/** @brief Construct an end iterator for @a list. */
	iterator(List<T, member> *list)
	    : const_iterator(list) {
	}
	/** @brief Construct an iterator pointing at @a x in @a list. */
	iterator(T *x, List<T, member> *list)
	    : const_iterator(x, list) {
	}
	/** @brief Return the current list element or null. */
	T *get() const {
	    return this->_x;
	}
	/** @brief Return the current list element or null. */
	T *operator->() const {
	    return this->_x;
	}
	/** @brief Return the current list element. */
	T &operator*() const {
	    return *this->_x;
	}
	/** @brief Move this iterator forward by @a x positions.
	 * @return reference to this iterator
	 * @note This function takes O(abs(@a x)) time. */
	iterator &operator+=(int x) {
	    for (; x > 0; --x)
		++*this;
	    for (; x < 0; ++x)
		--*this;
	    return *this;
	}
	/** @brief Move this iterator backward by @a x positions.
	 * @return reference to this iterator
	 * @note This function takes O(abs(@a x)) time. */
	iterator &operator-=(int x) {
	    for (; x > 0; --x)
		--*this;
	    for (; x < 0; ++x)
		++*this;
	    return *this;
	}
	/** @brief Return an iterator @a x positions ahead. */
	iterator operator+(int x) const {
	    iterator it(*this);
	    return it += x;
	}
	/** @brief Return an iterator @a x positions behind. */
	iterator operator-(int x) const {
	    iterator it(*this);
	    return it -= x;
	}
    };

  private:

    T *_head;
    T *_tail;

    List(const List<T, member> &x);
    List<T, member> &operator=(const List<T, member> &x);

};

#undef LIST_HEAD_MARKER
CLICK_ENDDECLS
#endif /* CLICK_LIST_HH */
