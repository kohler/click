// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ATOMIC_HH
#define CLICK_ATOMIC_HH
#if CLICK_LINUXMODULE
# include <click/glue.hh>
#endif
CLICK_DECLS
#if CLICK_LINUXMODULE
# if HAVE_LINUX_ASM_SYSTEM_H
#  include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#  include <asm/system.h>
CLICK_CXX_UNPROTECT
#  include <click/cxxunprotect.h>
# endif
# define CLICK_ATOMIC_VAL	_val.counter
#else
# define CLICK_ATOMIC_VAL	_val
#endif
#if defined(__i386__) || defined(__arch_um__) || defined(__x86_64__)
# if CLICK_LINUXMODULE || HAVE_MULTITHREAD
#  define CLICK_ATOMIC_X86	1
# endif
# if (CLICK_LINUXMODULE && defined(CONFIG_SMP)) || HAVE_MULTITHREAD
#  define CLICK_ATOMIC_LOCK	"lock ; "
# else
#  define CLICK_ATOMIC_LOCK	/* nothing */
# endif
#endif

/** @file <click/atomic.hh>
 * @brief An atomic 32-bit integer.
 */

/** @class atomic_uint32_t
 * @brief A 32-bit integer with support for atomic operations.
 *
 * The atomic_uint32_t class represents a 32-bit integer, with support for
 * atomic operations.  The +=, -=, &=, |=, ++, and -- operations are
 * implemented using atomic instructions.  There are also atomic swap(),
 * fetch_and_add(), dec_and_test(), and compare_swap() operations.
 *
 * Because of some issues with compiler implementations, atomic_uint32_t has
 * no explicit constructor; to set an atomic_uint32_t to a value, use
 * operator=.
 *
 * The atomic_uint32_t only provides true atomic semantics when that has been
 * implemented.  It has been implemented in the Linux kernel, and at userlevel
 * (when --enable-multithread has been defined) for x86 machines.  In other
 * situations, it's not truly atomic (because it doesn't need to be).
 */
class atomic_uint32_t { public:

    // No constructors because, unfortunately, GCC generates worse code. Use
    // operator= instead.

    inline uint32_t value() const;
    inline operator uint32_t() const;

    inline atomic_uint32_t &operator=(uint32_t x);

    inline atomic_uint32_t &operator+=(int32_t delta);
    inline atomic_uint32_t &operator-=(int32_t delta);
    inline atomic_uint32_t &operator|=(uint32_t mask);
    inline atomic_uint32_t &operator&=(uint32_t mask);

    inline void operator++();
    inline void operator++(int);
    inline void operator--();
    inline void operator--(int);

    inline uint32_t swap(uint32_t desired);
    inline uint32_t fetch_and_add(uint32_t delta);
    inline bool dec_and_test();
    inline uint32_t compare_swap(uint32_t expected, uint32_t desired);
    inline bool compare_and_swap(uint32_t expected, uint32_t desired) CLICK_DEPRECATED;

    inline static uint32_t swap(volatile uint32_t &x, uint32_t desired);
    inline static void inc(volatile uint32_t &x);
    inline static bool dec_and_test(volatile uint32_t &x);
    inline static uint32_t compare_swap(volatile uint32_t &x, uint32_t expected, uint32_t desired);
    inline static bool compare_and_swap(volatile uint32_t &x, uint32_t expected, uint32_t desired) CLICK_DEPRECATED;

  private:

#if CLICK_LINUXMODULE
    atomic_t _val;
#elif HAVE_MULTITHREAD
    volatile uint32_t _val;
#else
    uint32_t _val;
#endif

};

/** @brief  Return the value. */
inline uint32_t
atomic_uint32_t::value() const
{
#if CLICK_LINUXMODULE
    return atomic_read(&_val);
#else
    return CLICK_ATOMIC_VAL;
#endif
}

/** @brief  Return the value. */
inline
atomic_uint32_t::operator uint32_t() const
{
    return value();
}

/** @brief  Set the value to @a x. */
inline atomic_uint32_t &
atomic_uint32_t::operator=(uint32_t x)
{
#if CLICK_LINUXMODULE
    atomic_set(&_val, x);
#else
    CLICK_ATOMIC_VAL = x;
#endif
    return *this;
}

/** @brief  Atomically add @a delta to the value. */
inline atomic_uint32_t &
atomic_uint32_t::operator+=(int32_t delta)
{
#if CLICK_LINUXMODULE
    atomic_add(delta, &_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "addl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (delta), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL += delta;
#endif
    return *this;
}

/** @brief  Atomically subtract @a delta from the value. */
inline atomic_uint32_t &
atomic_uint32_t::operator-=(int32_t delta)
{
#if CLICK_LINUXMODULE
    atomic_sub(delta, &_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "subl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (delta), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL -= delta;
#endif
    return *this;
}

/** @brief  Atomically bitwise-or the value with @a mask. */
inline atomic_uint32_t &
atomic_uint32_t::operator|=(uint32_t mask)
{
#if CLICK_LINUXMODULE && HAVE_LINUX_ATOMIC_SET_MASK
    atomic_set_mask(mask, &_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "orl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (mask), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#elif CLICK_LINUXMODULE
# warning "using nonatomic approximation for atomic_uint32_t::operator|="
    unsigned long flags;
    local_irq_save(flags);
    CLICK_ATOMIC_VAL |= mask;
    local_irq_restore(flags);
#else
    CLICK_ATOMIC_VAL |= mask;
#endif
     return *this;
}

/** @brief  Atomically bitwise-and the value with @a mask. */
inline atomic_uint32_t &
atomic_uint32_t::operator&=(uint32_t mask)
{
#if CLICK_LINUXMODULE && HAVE_LINUX_ATOMIC_SET_MASK
    atomic_clear_mask(~mask, &_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "andl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (mask), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#elif CLICK_LINUXMODULE
# warning "using nonatomic approximation for atomic_uint32_t::operator&="
    unsigned long flags;
    local_irq_save(flags);
    CLICK_ATOMIC_VAL &= mask;
    local_irq_restore(flags);
#else
    CLICK_ATOMIC_VAL &= mask;
#endif
    return *this;
}

/** @brief  Atomically increment value @a x. */
inline void
atomic_uint32_t::inc(volatile uint32_t &x)
{
#if CLICK_LINUXMODULE
    static_assert(sizeof(atomic_t) == sizeof(x), "atomic_t expected to take 32 bits.");
    atomic_inc((atomic_t *) &x);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "incl %0"
		  : "=m" (x)
		  : "m" (x)
		  : "cc");
#else
    x++;
#endif
}

/** @brief  Atomically increment the value. */
inline void
atomic_uint32_t::operator++()
{
#if CLICK_LINUXMODULE
    atomic_inc(&_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "incl %0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL++;
#endif
}

/** @brief  Atomically increment the value. */
inline void
atomic_uint32_t::operator++(int)
{
#if CLICK_LINUXMODULE
    atomic_inc(&_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "incl %0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL++;
#endif
}

/** @brief  Atomically decrement the value. */
inline void
atomic_uint32_t::operator--()
{
#if CLICK_LINUXMODULE
    atomic_dec(&_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "decl %0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL--;
#endif
}

/** @brief  Atomically decrement the value. */
inline void
atomic_uint32_t::operator--(int)
{
#if CLICK_LINUXMODULE
    atomic_dec(&_val);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "decl %0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL--;
#endif
}

/** @brief  Atomically assign the value to @a desired, returning the old value.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t actual = x;
 * x = desired;
 * return actual;
 * @endcode
 *
 * Also acts as a memory barrier. */
inline uint32_t
atomic_uint32_t::swap(volatile uint32_t &x, uint32_t desired)
{
#if CLICK_ATOMIC_X86
    asm volatile ("xchgl %0,%1"
		  : "=r" (desired), "=m" (x)
		  : "0" (desired), "m" (x)
		  : "memory");
    return desired;
#elif CLICK_LINUXMODULE && defined(xchg)
    return xchg(&x, desired);
#elif CLICK_LINUXMODULE
# error "need xchg for atomic_uint32_t::swap"
#else
    uint32_t actual = x;
    x = desired;
    return actual;
#endif
}

/** @brief  Atomically assign the value to @a desired, returning the old value.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t old_value = value();
 * *this = desired;
 * return old_value;
 * @endcode
 *
 * Also acts as a memory barrier. */
inline uint32_t
atomic_uint32_t::swap(uint32_t desired)
{
#if CLICK_LINUXMODULE && defined(xchg)
    return atomic_xchg(&_val, desired);
#elif CLICK_LINUXMODULE
# error "need xchg for atomic_uint32_t::swap"
#else
    return swap(CLICK_ATOMIC_VAL, desired);
#endif
}

/** @brief  Atomically add @a delta to the value, returning the old value.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t old_value = value();
 * *this += delta;
 * return old_value;
 * @endcode */
inline uint32_t
atomic_uint32_t::fetch_and_add(uint32_t delta)
{
#if CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "xaddl %0,%1"
		  : "=r" (delta), "=m" (CLICK_ATOMIC_VAL)
		  : "0" (delta), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
    return delta;
#elif CLICK_LINUXMODULE && HAVE_LINUX_ATOMIC_ADD_RETURN
    return atomic_add_return(&_val, delta) - delta;
#elif CLICK_LINUXMODULE
# warning "using nonatomic approximation for atomic_uint32_t::fetch_and_add"
    unsigned long flags;
    local_irq_save(flags);
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL += delta;
    local_irq_restore(flags);
    return old_value;
#else
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL += delta;
    return old_value;
#endif
}

/** @brief  Atomically decrement @a x, returning true if the new @a x
 *	    is 0.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * --x;
 * return x == 0;
 * @endcode */
inline bool
atomic_uint32_t::dec_and_test(volatile uint32_t &x)
{
#if CLICK_LINUXMODULE
    static_assert(sizeof(atomic_t) == sizeof(x), "atomic_t expected to take 32 bits.");
    return atomic_dec_and_test((atomic_t *) &x);
#elif CLICK_ATOMIC_X86
    uint8_t result;
    asm volatile (CLICK_ATOMIC_LOCK "decl %0 ; sete %1"
		  : "=m" (x), "=qm" (result)
		  : "m" (x)
		  : "cc");
    return result;
#else
    return (--x == 0);
#endif
}

/** @brief  Perform a compare-and-swap operation.
 *  @param  x         value
 *  @param  expected  test value
 *  @param  desired   new value
 *  @return The actual old value.  If it equaled @a expected, @a x has been
 *	    set to @a desired.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t actual = x;
 * if (x == expected)
 *     x = desired;
 * return actual;
 * @endcode
 *
 * Also acts as a memory barrier. */
inline uint32_t
atomic_uint32_t::compare_swap(volatile uint32_t &x, uint32_t expected, uint32_t desired)
{
#if CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "cmpxchgl %2,%1"
		  : "=a" (expected), "=m" (x)
		  : "r" (desired), "0" (expected), "m" (x)
		  : "cc", "memory");
    return expected;
#elif CLICK_LINUXMODULE && defined(cmpxchg)
    return cmpxchg(&x, expected, desired);
#elif CLICK_LINUXMODULE
# warning "using nonatomic approximation for atomic_uint32_t::compare_and_swap"
    unsigned long flags;
    local_irq_save(flags);
    uint32_t actual = x;
    if (actual == expected)
	x = desired;
    local_irq_restore(flags);
    return actual;
#else
    uint32_t actual = x;
    if (actual == expected)
	x = desired;
    return actual;
#endif
}


/** @brief  Perform a compare-and-swap operation.
 *  @param  x         value
 *  @param  expected  test value
 *  @param  desired   new value
 *  @return True if the old @a x equaled @a expected (in which case @a x
 *	    was set to @a desired), false otherwise.
 *  @deprecated Use compare_swap instead.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t old_value = x;
 * if (x == expected)
 *     x = desired;
 * return old_value == expected;
 * @endcode
 *
 * Also acts as a memory barrier. */
inline bool
atomic_uint32_t::compare_and_swap(volatile uint32_t &x, uint32_t expected, uint32_t desired)
{
#if CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "cmpxchgl %2,%0 ; sete %%al"
		  : "=m" (x), "=a" (expected)
		  : "r" (desired), "m" (x), "a" (expected)
		  : "cc", "memory");
    return (uint8_t) expected;
#elif CLICK_LINUXMODULE && defined(cmpxchg)
    return cmpxchg(&x, expected, desired) == expected;
#elif CLICK_LINUXMODULE
# warning "using nonatomic approximation for atomic_uint32_t::compare_and_swap"
    unsigned long flags;
    local_irq_save(flags);
    uint32_t old_value = x;
    if (old_value == expected)
	x = desired;
    local_irq_restore(flags);
    return old_value == expected;
#else
    uint32_t old_value = x;
    if (old_value == expected)
	x = desired;
    return old_value == expected;
#endif
}

/** @brief  Atomically decrement the value, returning true if the new value
 *	    is 0.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * --*this;
 * return value() == 0;
 * @endcode */
inline bool
atomic_uint32_t::dec_and_test()
{
#if CLICK_LINUXMODULE
    return atomic_dec_and_test(&_val);
#elif CLICK_ATOMIC_X86
    uint8_t result;
    asm volatile (CLICK_ATOMIC_LOCK "decl %0 ; sete %1"
		  : "=m" (CLICK_ATOMIC_VAL), "=qm" (result)
		  : "m" (CLICK_ATOMIC_VAL)
		  : "cc");
    return result;
#else
    return (--CLICK_ATOMIC_VAL == 0);
#endif
}

/** @brief  Perform a compare-and-swap operation.
 *  @param  expected  test value
 *  @param  desired   new value
 *  @return The actual old value.  If @a expected is returned, the
 *          value has been set to @a desired.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t actual = value();
 * if (actual == expected)
 *     *this = desired;
 * return actual;
 * @endcode
 *
 * Also acts as a memory barrier. */
inline uint32_t
atomic_uint32_t::compare_swap(uint32_t expected, uint32_t desired)
{
#if CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "cmpxchgl %2,%1"
		  : "=a" (expected), "=m" (CLICK_ATOMIC_VAL)
		  : "r" (desired), "0" (expected), "m" (CLICK_ATOMIC_VAL)
		  : "cc", "memory");
    return expected;
#elif CLICK_LINUXMODULE && HAVE_LINUX_ATOMIC_CMPXCHG
    return atomic_cmpxchg(&_val, expected, desired);
#elif CLICK_LINUXMODULE
# warning "using nonatomic approximation for atomic_uint32_t::compare_swap"
    unsigned long flags;
    local_irq_save(flags);
    uint32_t actual = value();
    if (actual == expected)
	CLICK_ATOMIC_VAL = desired;
    local_irq_restore(flags);
    return actual;
#else
    uint32_t actual = value();
    if (actual == expected)
	CLICK_ATOMIC_VAL = desired;
    return actual;
#endif
}

/** @brief  Perform a compare-and-swap operation.
 *  @param  expected  test value
 *  @param  desired   new value
 *  @return True if the old value equaled @a expected (in which case the
 *	    value was set to @a desired), false otherwise.
 *  @deprecated  Use compare_swap instead.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t old_value = value();
 * if (old_value == expected)
 *     *this = desired;
 * return old_value == expected;
 * @endcode
 *
 * Also acts as a memory barrier. */
inline bool
atomic_uint32_t::compare_and_swap(uint32_t expected, uint32_t desired)
{
#if CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "cmpxchgl %2,%0 ; sete %%al"
		  : "=m" (CLICK_ATOMIC_VAL), "=a" (expected)
		  : "r" (desired), "m" (CLICK_ATOMIC_VAL), "a" (expected)
		  : "cc", "memory");
    return (uint8_t) expected;
#elif CLICK_LINUXMODULE && HAVE_LINUX_ATOMIC_CMPXCHG
    return atomic_cmpxchg(&_val, expected, desired) == expected;
#elif CLICK_LINUXMODULE
# warning "using nonatomic approximation for atomic_uint32_t::compare_and_swap"
    unsigned long flags;
    local_irq_save(flags);
    uint32_t old_value = value();
    if (old_value == expected)
	CLICK_ATOMIC_VAL = desired;
    local_irq_restore(flags);
    return old_value == expected;
#else
    uint32_t old_value = value();
    if (old_value == expected)
	CLICK_ATOMIC_VAL = desired;
    return old_value == expected;
#endif
}

inline uint32_t
operator+(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() + b.value();
}

inline uint32_t
operator-(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() - b.value();
}

inline bool
operator==(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() == b.value();
}

inline bool
operator!=(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() != b.value();
}

inline bool
operator>(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() > b.value();
}

inline bool
operator<(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() < b.value();
}

inline bool
operator>=(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() >= b.value();
}

inline bool
operator<=(const atomic_uint32_t &a, const atomic_uint32_t &b)
{
    return a.value() <= b.value();
}

typedef atomic_uint32_t uatomic32_t;

CLICK_ENDDECLS
#endif
