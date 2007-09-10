// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ATOMIC_HH
#define CLICK_ATOMIC_HH
CLICK_DECLS
#if CLICK_LINUXMODULE
# include <click/glue.hh>
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#  define CLICK_ATOMIC_VAL	_val.counter
# else
#  define CLICK_ATOMIC_VAL	__atomic_fool_gcc(&_val)
# endif
# ifdef __SMP__
#  define CLICK_ATOMIC_LOCK	"lock ; "
# else
#  define CLICK_ATOMIC_LOCK	/* nothing */
# endif
#else
# define CLICK_ATOMIC_VAL	_val
# if HAVE_MULTITHREAD && defined(__i386__)
#  define CLICK_ATOMIC_X86	1
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
 * fetch_and_add(), dec_and_test(), and compare_and_swap() operations.
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
  
    inline atomic_uint32_t &operator=(uint32_t v);

    inline atomic_uint32_t &operator+=(int32_t delta);
    inline atomic_uint32_t &operator-=(int32_t delta);
    inline atomic_uint32_t &operator|=(uint32_t mask);
    inline atomic_uint32_t &operator&=(uint32_t mask);

    inline void operator++();
    inline void operator++(int);
    inline void operator--();
    inline void operator--(int);

    inline uint32_t swap(uint32_t v);
    inline uint32_t fetch_and_add(uint32_t delta);
    inline bool dec_and_test();
    inline bool compare_and_swap(uint32_t test_value, uint32_t new_value);
  
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

/** @brief  Set the value to @a v. */
inline atomic_uint32_t &
atomic_uint32_t::operator=(uint32_t v)
{
#if CLICK_LINUXMODULE
    atomic_set(&_val, v);
#else
    CLICK_ATOMIC_VAL = v;
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
#elif CLICK_LINUXMODULE
    unsigned long flags;
    local_irq_save(flags);
    CLICK_ATOMIC_VAL |= mask;
    local_irq_restore(flags);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "orl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (mask), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
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
#elif CLICK_LINUXMODULE
    unsigned long flags;
    local_irq_save(flags);
    CLICK_ATOMIC_VAL &= mask;
    local_irq_restore(flags);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "andl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (mask), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL &= mask;
#endif
    return *this;
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

/** @brief  Atomically assign the value to @a v, returning the old value.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t old_value = value();
 * *this = v;
 * return old_value;
 * @endcode*/
inline uint32_t
atomic_uint32_t::swap(uint32_t v)
{
#if (CLICK_LINUXMODULE && (defined(__i386__) || defined(__arch_um__) || defined(__x86_64__))) || CLICK_ATOMIC_X86
    asm ("xchgl %0,%1"
	 : "=r" (v), "=m" (CLICK_ATOMIC_VAL));
    return v;
#elif CLICK_LINUXMODULE
    unsigned long flags;
    local_irq_save(flags);
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL = v;
    local_irq_restore(flags);
    return old_value;
#else
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL = v;
    return old_value;
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
#if (CLICK_LINUXMODULE && (defined(__i386__) || defined(__arch_um__) || defined(__x86_64__))) || CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "xaddl %0,%1"
		  : "=r" (delta), "=m" (CLICK_ATOMIC_VAL) 
		  : "0" (delta), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
    return delta;
#elif CLICK_LINUXMODULE
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
 *  @param  test_value  test value
 *  @param  new_value   new value
 *  @return True if the old value equaled @a test_value (in which case the
 *	    value was set to @a new_value), false otherwise.
 *
 * Behaves like this, but in one atomic step:
 * @code
 * uint32_t old_value = value();
 * if (old_value == test_value)
 *     *this = new_value;
 * return old_value == test_value;
 * @endcode */
inline bool
atomic_uint32_t::compare_and_swap(uint32_t test_value, uint32_t new_value)
{
#if (CLICK_LINUXMODULE && (defined(__i386__) || defined(__arch_um__) || defined(__x86_64__))) || CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "cmpxchgl %2,%0 ; sete %%al"
		  : "=m" (CLICK_ATOMIC_VAL), "=a" (test_value)
		  : "r" (new_value), "m" (CLICK_ATOMIC_VAL), "a" (test_value)
		  : "cc");
    return (uint8_t) test_value;
#elif CLICK_LINUXMODULE
    unsigned long flags;
    local_irq_save(flags);
    uint32_t old_value = value();
    if (old_value == test_value)
	CLICK_ATOMIC_VAL = new_value;
    local_irq_restore(flags);
    return old_value == test_value;
#else
    uint32_t old_value = value();
    if (old_value == test_value)
	CLICK_ATOMIC_VAL = new_value;
    return old_value == test_value;
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
