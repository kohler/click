// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PAIR_HH
#define CLICK_PAIR_HH

template <class T, class U>
struct Pair {
    T first;
    U second;
    Pair()				: first(), second() { }
    Pair(const T &t, const U &u)	: first(t), second(u) { }
    inline operator bool() const;
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
inline unsigned hashcode(const Pair<T, U> &a)
{
    return (hashcode(a.first) << 13) ^ hashcode(a.second);
}

template <class T, class U>
inline Pair<T, U> make_pair(const T &t, const U &u)
{
    return Pair<T, U>(t, u);
}

#endif
