#ifndef CLICK_ALGORITHM_HH
#define CLICK_ALGORITHM_HH
CLICK_DECLS

template <typename T>
inline T *
find(T *begin, T *end, const T &val)
{
    while (begin < end && *begin != val)
	begin++;
    return begin;
}

template <typename T>
inline const T *
find(const T *begin, const T *end, const T &val)
{
    while (begin < end && *begin != val)
	begin++;
    return begin;
}

template <typename T>
inline void
ignore_result(T result)
{
    (void) result;
}

CLICK_ENDDECLS
#endif
