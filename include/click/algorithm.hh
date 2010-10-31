#ifndef CLICK_ALGORITHM_HH
#define CLICK_ALGORITHM_HH
CLICK_DECLS

template <typename T>
inline T *find(T *begin, T *end, const T &val)
{
    while (begin < end && *begin != val)
	++begin;
    return begin;
}

template <typename T>
inline const T *find(const T *begin, const T *end, const T &val)
{
    while (begin < end && *begin != val)
	++begin;
    return begin;
}

template <typename T>
inline void ignore_result(T result)
{
    (void) result;
}

/** @brief Exchange the values of @a a and @a b.
 *
 * The generic version constructs a temporary copy of @a a.  Some
 * specializations avoid this copy. */
template <typename T>
inline void click_swap(T &a, T &b)
{
    T tmp(a);
    a = b;
    b = tmp;
}

/** @brief Replace @a x with a default-constructed object.
 *
 * Unlike @a x.clear(), this function usually frees all memory associated with
 * @a x. */
template <typename T>
inline void clear_by_swap(T &x)
{
    T tmp;
    click_swap(x, tmp);
}


/** @brief Function object that does nothing when called. */
template <typename T>
struct do_nothing {
    typedef T argument_type;
    typedef void result_type;
    void operator()(const T &) {
    }
};

/** @brief Function object that encapsulates operator<(). */
template <typename T>
struct less {
    typedef T first_argument_type;
    typedef T second_argument_type;
    typedef bool result_type;
    bool operator()(const T &x, const T &y) {
	return x < y;
    }
};

CLICK_ENDDECLS
#endif
