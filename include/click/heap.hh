#ifndef CLICK_HEAP_HH
#define CLICK_HEAP_HH
#include <click/algorithm.hh>
CLICK_DECLS

/** @brief Add an element to a heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \< @a end
 * @pre [@a begin, @a end - 1) is a heap
 * @post [@a begin, @a end) is a heap
 *
 * This function rearranges the elements in [@a begin, @a end) to be a heap.
 * It assumes that most of the sequence is already a heap -- only the new
 * element, @a end[-1], might not be in a valid place.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each element that
 * changes place within the heap order; its arguments are @a begin and
 * an iterator for the element that switched place.  @a place is always
 * called once with an iterator pointing the new element in its final
 * place.  @a place is useful when elements need to keep track of
 * their own positions in the heap order.  @a place defaults to
 * do_nothing<>().
 *
 * @sa change_heap, pop_heap, remove_heap */
template <typename iterator_type, typename compare_type, typename place_type>
inline void push_heap(iterator_type begin, iterator_type end,
		      compare_type comp, place_type place)
{
    assert(begin < end);
    size_t i = end - begin - 1, npos;

    while (i > 0 && (npos = (i-1)/2, comp(begin[i], begin[npos]))) {
	click_swap(begin[i], begin[npos]);
	place(begin, begin + i);
	i = npos;
    }

    place(begin, begin + i);
}

/** @overload */
template <typename iterator_type, typename compare_type>
inline void push_heap(iterator_type begin, iterator_type end,
		      compare_type comp)
{
    push_heap(begin, end, comp, do_nothing<iterator_type, iterator_type>());
}

/** @brief Change an element's position within a heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param element iterator pointing to element whose position may change
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \<= @a element < @a end
 * @pre [@a begin, @a end) is a heap, perhaps excluding @a element
 * @post [@a begin, @a end) is a heap
 * @return iterator pointing to the new location of *@a element
 *
 * This function rearranges the elements in [@a begin, @a end) to be a heap.
 * It assumes that most of the sequence is already a heap.  Only the element
 * pointed to by @a element might be out of place.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each element that
 * changes place within the heap order; its arguments are @a begin and
 * an iterator for the element that switched place.  @a place is
 * useful when elements need to keep track of their own positions in
 * the heap order.  @a place defaults to do_nothing<>().
 *
 * @sa push_heap, pop_heap, remove_heap */
template <typename iterator_type, typename compare_type, typename place_type>
iterator_type change_heap(iterator_type begin, iterator_type end,
			  iterator_type element,
			  compare_type comp, place_type place)
{
    assert(begin <= element && element < end);
    size_t i = element - begin, size = end - begin, npos;

    while (i > 0 && (npos = (i-1)/2, comp(begin[i], begin[npos]))) {
	click_swap(begin[i], begin[npos]);
	place(begin, begin + i);
	i = npos;
    }

    while (1) {
	size_t smallest = i, trial = i*2 + 1;
        if (trial < size && comp(begin[trial], begin[smallest]))
            smallest = trial;
        if (trial + 1 < size && comp(begin[trial + 1], begin[smallest]))
            smallest = trial + 1;
        if (smallest == i)
            break;
	click_swap(begin[i], begin[smallest]);
	place(begin, begin + i);
        i = smallest;
    }

    if (begin + i != element)
	place(begin, begin + i);
    return begin + i;
}

/** @overload */
template <typename iterator_type, typename compare_type>
inline iterator_type change_heap(iterator_type begin, iterator_type end,
				 iterator_type element, compare_type comp)
{
    return change_heap(begin, end, element, comp, do_nothing<iterator_type, iterator_type>());
}

/** @brief Remove an element from a heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param element iterator pointing to element to remove
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \<= @a element < @a end
 * @pre [@a begin, @a end) is a heap, possibly excluding @a element
 * @post [@a begin, @a end - 1) is a heap, and the element formerly at
 * *@a element has shifted to *(@a end - 1)
 *
 * This function removes @a element from the heap in [@a begin, @a end) by
 * shifting it to the end, preserving the heap property on the remaining
 * elements.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each actual element
 * that changes place within the heap order; its arguments are @a
 * begin and an iterator for the element that switched place.  It is
 * not called on @a element, which is no longer considered a member of
 * the heap.  @a place is useful when elements need to keep track of
 * their own positions in the heap order.  @a place defaults to
 * do_nothing<>().
 *
 * @sa push_heap, change_heap, pop_heap */
template <typename iterator_type, typename compare_type, typename place_type>
inline void remove_heap(iterator_type begin, iterator_type end,
			iterator_type element,
			compare_type comp, place_type place)
{
    assert(begin <= element && element < end);
    if (element + 1 != end) {
	click_swap(element[0], end[-1]);
	place(begin, element);
	change_heap(begin, end - 1, element, comp, place);
    }
}

/** @overload */
template <typename iterator_type, typename compare_type>
inline void remove_heap(iterator_type begin, iterator_type end,
			iterator_type element,
			compare_type comp)
{
    remove_heap(begin, end, element, comp, do_nothing<iterator_type, iterator_type>());
}

/** @brief Remove the first element from a heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \< @a end
 * @pre [@a begin, @a end) is a heap
 * @post [@a begin, @a end - 1) is a heap, and the element formerly at
 * *@a begin has shifted to *(@a end - 1)
 *
 * This function removes the first element of [@a begin, @a end) from a heap
 * by shifting it to the end, preserving the heap property on the remaining
 * elements.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each element that
 * changes place within the heap order; its arguments are @a begin and
 * an iterator for the element that switched place.  It is not called
 * on the first element, which is no longer considered a member of the
 * heap.  @a place is useful when elements need to keep track of their
 * own positions in the heap order.  @a place defaults to
 * do_nothing<>().
 *
 * @sa push_heap, change_heap, remove_heap */
template <typename iterator_type, typename compare_type, typename place_type>
inline void pop_heap(iterator_type begin, iterator_type end,
		     compare_type comp, place_type place)
{
    remove_heap(begin, end, begin, comp, place);
}

/** @overload */
template <typename iterator_type, typename compare_type>
inline void pop_heap(iterator_type begin, iterator_type end,
		     compare_type comp)
{
    pop_heap(begin, end, comp, do_nothing<iterator_type, iterator_type>());
}



/** @brief Add an element to a d-ary heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \< @a end
 * @pre [@a begin, @a end - 1) is a d-ary heap
 * @post [@a begin, @a end) is a d-ary heap
 *
 * This function rearranges the elements in [@a begin, @a end) to be
 * a d-ary heap.  It assumes that most of the sequence is already a
 * heap -- only the new element, @a end[-1], might not be in a valid
 * place.  The arity @a n is a template parameter, and must be greater
 * than 2.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each element that
 * changes place within the heap order; its arguments are @a begin and
 * an iterator for the element that switched place.  @a place is always
 * called once with an iterator pointing the new element in its final
 * place.  @a place is useful when elements need to keep track of
 * their own positions in the heap order.  @a place defaults to
 * do_nothing<>().
 *
 * @sa change_heap, pop_heap, remove_heap */
template <int arity,
	  typename iterator_type, typename compare_type, typename place_type>
inline void push_heap(iterator_type begin, iterator_type end,
		      compare_type comp, place_type place)
{
    static_assert(arity > 2, "For arity == 2, use push_heap(...), not push_heap<2>(...).");
    assert(begin < end);
    size_t i = end - begin - 1, npos;

    while (i > 0 && (npos = i/arity, comp(begin[i], begin[npos]))) {
	click_swap(begin[i], begin[npos]);
	place(begin, begin + i);
	i = npos;
    }

    place(begin, begin + i);
}

/** @overload */
template <int arity, typename iterator_type, typename compare_type>
inline void push_heap(iterator_type begin, iterator_type end,
		      compare_type comp)
{
    push_heap<arity>(begin, end, comp, do_nothing<iterator_type, iterator_type>());
}

/** @brief Change an element's position within a d-ary heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param element iterator pointing to element whose position may change
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \<= @a element < @a end
 * @pre [@a begin, @a end) is a d-ary heap, perhaps excluding @a element
 * @post [@a begin, @a end) is a d-ary heap
 * @return iterator pointing to the new location of *@a element
 *
 * This function rearranges the elements in [@a begin, @a end) to be
 * a d-ary heap.  It assumes that most of the sequence is already a
 * heap.  Only the element pointed to by @a element might be out of
 * place.  The arity @a n is a template parameter, and must be greater
 * than 2.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each element that
 * changes place within the heap order; its arguments are @a begin and
 * an iterator for the element that switched place.  @a place is
 * useful when elements need to keep track of their own positions in
 * the heap order.  @a place defaults to do_nothing<>().
 *
 * @sa push_heap, pop_heap, remove_heap */
template <int arity,
	  typename iterator_type, typename compare_type, typename place_type>
iterator_type change_heap(iterator_type begin, iterator_type end,
			  iterator_type element,
			  compare_type comp, place_type place)
{
    static_assert(arity > 2, "For arity == 2, use change_heap(...), not change_heap<2>(...).");
    assert(begin <= element && element < end);
    size_t i = element - begin, size = end - begin, npos;

    while (i > 0 && (npos = i/arity, comp(begin[i], begin[npos]))) {
	click_swap(begin[i], begin[npos]);
	place(begin, begin + i);
	i = npos;
    }

    while (1) {
	size_t smallest = i,
	    trial = i ? i * arity : 1,
	    end_trial = i ? trial + arity : arity;
	end_trial = end_trial < size ? end_trial : size;
	for (; trial < end_trial; ++trial)
	    if (comp(begin[trial], begin[smallest]))
		smallest = trial;
        if (smallest == i)
            break;
	click_swap(begin[i], begin[smallest]);
	place(begin, begin + i);
        i = smallest;
    }

    if (begin + i != element)
	place(begin, begin + i);
    return begin + i;
}

/** @overload */
template <int arity,
	  typename iterator_type, typename compare_type>
inline iterator_type change_heap(iterator_type begin, iterator_type end,
				 iterator_type element, compare_type comp)
{
    return change_heap<arity>(begin, end, element, comp, do_nothing<iterator_type, iterator_type>());
}

/** @brief Remove an element from a d-ary heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param element iterator pointing to element to remove
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \<= @a element < @a end
 * @pre [@a begin, @a end) is a d-ary heap, possibly excluding @a element
 * @post [@a begin, @a end - 1) is a d-ary heap, and the element formerly
 * at *@a element has shifted to *(@a end - 1)
 *
 * This function removes @a element from the d-ary heap in [@a begin,
 * @a end) by shifting it to the end, preserving the heap property on
 * the remaining elements.  The arity @a n is a template parameter.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each actual element
 * that changes place within the heap order; its arguments are @a
 * begin and an iterator for the element that switched place.  It is
 * not called on @a element, which is no longer considered a member of
 * the heap.  @a place is useful when elements need to keep track of
 * their own positions in the heap order.  @a place defaults to
 * do_nothing<>().
 *
 * @sa push_heap, change_heap, pop_heap */
template <int arity,
	  typename iterator_type, typename compare_type, typename place_type>
inline void remove_heap(iterator_type begin, iterator_type end,
			iterator_type element,
			compare_type comp, place_type place)
{
    assert(begin <= element && element < end);
    if (element + 1 != end) {
	click_swap(element[0], end[-1]);
	place(begin, element);
	change_heap<arity>(begin, end - 1, element, comp, place);
    }
}

/** @overload */
template <int arity,
	  typename iterator_type, typename compare_type>
inline void remove_heap(iterator_type begin, iterator_type end,
			iterator_type element,
			compare_type comp)
{
    remove_heap<arity>(begin, end, element, comp, do_nothing<iterator_type, iterator_type>());
}

/** @brief Remove the first element from a d-ary heap.
 * @param begin begin random-access iterator
 * @param end end random-access iterator
 * @param comp compare function object, such as less<>
 * @param place placement function object, defaults to do_nothing<>
 * @pre @a begin \< @a end
 * @pre [@a begin, @a end) is a d-ary heap
 * @post [@a begin, @a end - 1) is a d-ary heap, and the element formerly
 * at *@a begin has shifted to *(@a end - 1)
 *
 * This function removes the first element of [@a begin, @a end) from
 * a d-ary heap by shifting it to the end, preserving the heap
 * property on the remaining elements.  The arity @a n is a template
 * parameter.
 *
 * The comparison function @a comp defines the heap order.
 *
 * The placement function @a place is called for each element that
 * changes place within the heap order; its arguments are @a begin and
 * an iterator for the element that switched place.  It is not called
 * on the first element, which is no longer considered a member of the
 * heap.  @a place is useful when elements need to keep track of their
 * own positions in the heap order.  @a place defaults to
 * do_nothing<>().
 *
 * @sa push_heap, change_heap, remove_heap */
template <int arity,
	  typename iterator_type, typename compare_type, typename place_type>
inline void pop_heap(iterator_type begin, iterator_type end,
		     compare_type comp, place_type place)
{
    remove_heap<arity>(begin, end, begin, comp, place);
}

/** @overload */
template <int arity,
	  typename iterator_type, typename compare_type>
inline void pop_heap(iterator_type begin, iterator_type end,
		     compare_type comp)
{
    pop_heap<arity>(begin, end, comp, do_nothing<iterator_type, iterator_type>());
}

CLICK_ENDDECLS
#endif
