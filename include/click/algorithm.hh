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

/** @brief Assign @a x to a copy of @a y, possibly modifying @a y.
 *
 * This is like @a x = @a y, except that under certain circumstances
 * it can modify @a y (for example, by calling @a x.swap(@a y)). */
template <typename T, typename V>
inline void assign_consume(T &x, const V &y)
{
    x = y;
}


template <typename T, typename U = void> struct do_nothing;

/** @brief Binary function object that does nothing when called. */
template <typename T, typename U>
struct do_nothing {
    typedef T first_argument_type;
    typedef U second_argument_type;
    typedef void result_type;
    void operator()(const T &, const U &) {
    }
};

/** @brief Unary function object that does nothing when called. */
template <typename T>
struct do_nothing<T, void> {
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

#define click_min(a, b) ((a) < (b) ? (a) : (b))
#define click_max(a, b) ((a) > (b) ? (a) : (b))

CLICK_ENDDECLS
#endif
