#ifndef CLICK_ARRAY_MEMORY_HH
#define CLICK_ARRAY_MEMORY_HH 1
#include <click/glue.hh>
#include <click/type_traits.hh>
#if HAVE_VALGRIND && HAVE_VALGRIND_MEMCHECK_H
# include <valgrind/memcheck.h>
#endif
CLICK_DECLS

template <size_t s> class sized_array_memory { public:
    typedef char_array<s> type;
    template <typename T> static type *cast(T *x) {
	static_assert(sizeof(type) == s, "char_array<> size off");
	return reinterpret_cast<type *>(x);
    }
    template <typename T> static const type *cast(const T *x) {
	return reinterpret_cast<const type *>(x);
    }
    static void fill(void *a, size_t n, const void *x) {
	for (; n != 0; --n, a = (char *) a + s)
	    memcpy(a, x, s);
    }
    static void move_construct(void* a, void* x) {
	memcpy(a, x, s);
    }
    static void copy(void *dst, const void *src, size_t n) {
        if (n)
            memcpy(dst, src, n * s);
    }
    static void move(void *dst, const void *src, size_t n) {
        if (n)
            memmove(dst, src, n * s);
    }
    static void move_onto(void *dst, const void *src, size_t n) {
        if (n)
            memmove(dst, src, n * s);
    }
    static void destroy(void *a, size_t n) {
	(void) a, (void) n;
    }
    static void mark_noaccess(void *a, size_t n) {
#ifdef VALGRIND_MAKE_MEM_NOACCESS
	VALGRIND_MAKE_MEM_NOACCESS(a, n * s);
#else
	(void) a, (void) n;
#endif
    }
    static void mark_undefined(void *a, size_t n) {
#ifdef VALGRIND_MAKE_MEM_UNDEFINED
	VALGRIND_MAKE_MEM_UNDEFINED(a, n * s);
#else
	(void) a, (void) n;
#endif
    }
};

template <typename T> class typed_array_memory { public:
    typedef T type;
    static T *cast(T *x) {
	return x;
    }
    static const T *cast(const T *x) {
	return x;
    }
    static void fill(T *a, size_t n, const T *x) {
	for (size_t i = 0; i != n; ++i)
	    new((void *) &a[i]) T(*x);
    }
    static void move_construct(T* a, T* x) {
#if HAVE_CXX_RVALUE_REFERENCES
	new((void *) a) T(click_move(*x));
#else
	new((void *) a) T(*x);
#endif
    }
    static void copy(T *dst, const T *src, size_t n) {
	for (size_t i = 0; i != n; ++i)
	    new((void *) &dst[i]) T(src[i]);
    }
    static void move(T *dst, const T *src, size_t n) {
	if (dst > src && src + n > dst) {
	    for (dst += n - 1, src += n - 1; n != 0; --n, --dst, --src) {
		new((void *) dst) T(*src);
		src->~T();
	    }
	} else {
	    for (size_t i = 0; i != n; ++i) {
		new((void *) &dst[i]) T(src[i]);
		src[i].~T();
	    }
	}
    }
    static void move_onto(T *dst, const T *src, size_t n) {
	if (dst > src && src + n > dst) {
	    for (dst += n - 1, src += n - 1; n != 0; --n, --dst, --src) {
		dst->~T();
		new((void *) dst) T(*src);
	    }
	} else {
	    for (size_t i = 0; i != n; ++i) {
		dst[i].~T();
		new((void *) &dst[i]) T(src[i]);
	    }
	}
    }
    static void destroy(T *a, size_t n) {
	for (size_t i = 0; i != n; ++i)
	    a[i].~T();
    }
    static void mark_noaccess(T *a, size_t n) {
	sized_array_memory<sizeof(T)>::mark_noaccess(a, n);
    }
    static void mark_undefined(T *a, size_t n) {
	sized_array_memory<sizeof(T)>::mark_undefined(a, n);
    }
};

template <typename T> class array_memory : public conditional<has_trivial_copy<T>::value, sized_array_memory<sizeof(T)>, typed_array_memory<T> > {};

CLICK_ENDDECLS
#endif
