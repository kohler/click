// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PAIR_HH
#define CLICK_PAIR_HH
#include <click/hashcode.hh>
CLICK_DECLS

template <class T, class U>
struct Pair {
    T first;
    U second;
    Pair()				: first(), second() { }
    Pair(const T &t, const U &u)	: first(t), second(u) { }
    inline operator bool() const;
    inline size_t hashcode() const;
};

template <class T, class U>
inline Pair<T, U>::operator bool() const
{
    return (bool) first || (bool) second;
}

template <class T, class U>
inline bool operator==(const Pair<T, U> &a, const Pair<T, U> &b)
{
    return a.first == b.first && a.second == b.second;
}

template <class T, class U>
inline bool operator!=(const Pair<T, U> &a, const Pair<T, U> &b)
{
    return a.first != b.first || a.second != b.second;
}

template <class T, class U>
inline bool operator<(const Pair<T, U> &a, const Pair<T, U> &b)
{
    return a.first < b.first
	|| (!(b.first < a.first) && a.second < b.second);
}

template <class T, class U>
inline size_t Pair<T, U>::hashcode() const
{
    return (CLICK_NAME(hashcode)(first) << 13) ^ CLICK_NAME(hashcode)(second);
}

template <class T, class U>
inline Pair<T, U> make_pair(const T &t, const U &u)
{
    return Pair<T, U>(t, u);
}

CLICK_ENDDECLS
#endif
