#ifndef CLICK_ALGORITHM_HH
#define CLICK_ALGORITHM_HH
CLICK_DECLS

template <typename T>
inline T *find(T *begin, T *end, const T &val)
{
    while (begin < end && *begin != val)
	begin++;
    return begin;
}

template <typename T>
inline const T *find(const T *begin, const T *end, const T &val)
{
    while (begin < end && *begin != val)
	begin++;
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
inline void swap(T &a, T &b)
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
    swap(x, tmp);
}

CLICK_ENDDECLS
#endif
