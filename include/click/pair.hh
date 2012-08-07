// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PAIR_HH
#define CLICK_PAIR_HH
#include <click/hashcode.hh>
#include <click/type_traits.hh>
CLICK_DECLS

template <class T, class U>
struct Pair {

    typedef T first_type;
    typedef U second_type;
    typedef T key_type;
    typedef const T &key_const_reference;

    T first;
    U second;

    inline Pair()
	: first(), second() {
    }

    inline Pair(typename fast_argument<T>::type t,
		typename fast_argument<U>::type u)
	: first(t), second(u) {
    }

    inline Pair(const Pair<T, U> &p)
	: first(p.first), second(p.second) {
    }

    template <typename V, typename W>
    inline Pair(const Pair<V, W> &p)
	: first(p.first), second(p.second) {
    }

    typedef hashcode_t (Pair<T, U>::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const {
	return first || second ? &Pair<T, U>::hashcode : 0;
    }

    inline const T &hashkey() const {
	return first;
    }

    inline hashcode_t hashcode() const;

    template <typename V, typename W>
    Pair<T, U> &operator=(const Pair<V, W> &p) {
	first = p.first;
	second = p.second;
	return *this;
    }

};

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
inline hashcode_t Pair<T, U>::hashcode() const
{
    return (CLICK_NAME(hashcode)(first) << 7) ^ CLICK_NAME(hashcode)(second);
}

template <class T, class U>
inline Pair<T, U> make_pair(T t, U u)
{
    return Pair<T, U>(t, u);
}

CLICK_ENDDECLS
#endif
