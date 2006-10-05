#ifndef CLICK_ALGORITHM_HH
#define CLICK_ALGORITHM_HH
CLICK_DECLS

template <class T>
inline T *
find(T *begin, T *end, const T &val)
{
    while (begin < end && *begin != val)
	begin++;
    return begin;
}

template <class T>
inline const T *
find(const T *begin, const T *end, const T &val)
{
    while (begin < end && *begin != val)
	begin++;
    return begin;
}

CLICK_ENDDECLS
#endif
