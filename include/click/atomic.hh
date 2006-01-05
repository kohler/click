// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ATOMIC_HH
#define CLICK_ATOMIC_HH
CLICK_DECLS
#if CLICK_LINUXMODULE
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
# if CLICK_USERLEVEL && defined(__i386__)
#  define CLICK_ATOMIC_X86	1
#  define CLICK_ATOMIC_LOCK	"lock ; "
# else
#  define CLICK_ATOMIC_LOCK	/* nothing */
# endif
#endif

class atomic_uint32_t { public:

    // No constructors because, unfortunately, GCC generates worse code. Use
    // operator= instead.
  
    inline uint32_t value() const;
    inline operator uint32_t() const;
  
    inline atomic_uint32_t &operator=(uint32_t);

    inline atomic_uint32_t &operator+=(int32_t);
    inline atomic_uint32_t &operator-=(int32_t);
    inline atomic_uint32_t &operator|=(uint32_t);
    inline atomic_uint32_t &operator&=(uint32_t);

    inline void operator++(int);
    inline void operator--(int);

    inline uint32_t swap(uint32_t new_value);
    inline uint32_t fetch_and_add(uint32_t delta);
    inline bool dec_and_test();
    inline uint32_t compare_and_swap(uint32_t test, uint32_t new_value);
  
  private:

#if CLICK_LINUXMODULE
    atomic_t _val;
#else
    uint32_t _val;
#endif

};

inline uint32_t
atomic_uint32_t::value() const
{
#if CLICK_LINUXMODULE
    return atomic_read(&_val);
#else
    return CLICK_ATOMIC_VAL;
#endif
}

inline
atomic_uint32_t::operator uint32_t() const
{
    return value();
}

inline atomic_uint32_t &
atomic_uint32_t::operator=(uint32_t u)
{
#if CLICK_LINUXMODULE
    atomic_set(&_val, u);
#else
    CLICK_ATOMIC_VAL = u;
#endif
    return *this;
}

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

inline atomic_uint32_t &
atomic_uint32_t::operator|=(uint32_t u)
{
#if CLICK_LINUXMODULE && !defined(__arm__)
    atomic_set_mask(u, &_val);
#elif CLICK_LINUXMODULE
    unsigned long flags;
    __save_flags_cli(flags);
    CLICK_ATOMIC_VAL |= u;
    __restore_flags(flags);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "orl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (u), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL |= u;
#endif
     return *this;
}

inline atomic_uint32_t &
atomic_uint32_t::operator&=(uint32_t u)
{
#if CLICK_LINUXMODULE && !defined(__arm__)
    atomic_clear_mask(~u, &_val);
#elif CLICK_LINUXMODULE
    unsigned long flags;
    __save_flags_cli(flags);
    CLICK_ATOMIC_VAL &= u;
    __restore_flags(flags);
#elif CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "andl %1,%0"
		  : "=m" (CLICK_ATOMIC_VAL)
		  : "r" (u), "m" (CLICK_ATOMIC_VAL)
		  : "cc");
#else
    CLICK_ATOMIC_VAL &= u;
#endif
    return *this;
}

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

inline uint32_t
atomic_uint32_t::swap(uint32_t new_value)
{
#if (CLICK_LINUXMODULE && (defined(__i386__) || defined(__arch_um__) || defined(__x86_64__))) || CLICK_ATOMIC_X86
    asm ("xchgl %0,%1"
	 : "=r" (new_value), "=m" (CLICK_ATOMIC_VAL));
    return new_value;
#elif CLICK_LINUXMODULE
    unsigned long flags;
    __save_flags_cli(flags);
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL = new_value;
    __restore_flags(flags);
    return old_value;
#else
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL = new_value;
    return old_value;
#endif
}

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
    __save_flags_cli(flags);
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL += delta;
    __restore_flags(flags);
    return old_value;
#else
    uint32_t old_value = value();
    CLICK_ATOMIC_VAL += delta;
    return old_value;
#endif
}

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
    return result != 0;
#else
    return (--CLICK_ATOMIC_VAL == 0);
#endif
}

inline uint32_t
atomic_uint32_t::compare_and_swap(uint32_t test_value, uint32_t new_value)
{
    // Pseudocode:
    //   begin_atomic_section();
    //   uint32_t old_value = *this;
    //   if (*this == test_value)
    //       *this = new_value;
    //   end_atomic_section();
    //   return old_value;

    uint32_t old_value;
#if (CLICK_LINUXMODULE && (defined(__i386__) || defined(__arch_um__) || defined(__x86_64__))) || CLICK_ATOMIC_X86
    asm volatile (CLICK_ATOMIC_LOCK "cmpxchgl %2,%0"
		  : "=m" (CLICK_ATOMIC_VAL), "=a" (old_value)
		  : "r" (new_value), "m" (CLICK_ATOMIC_VAL), "a" (test_value)
		  : "cc");
#elif CLICK_LINUXMODULE
    unsigned long flags;
    __save_flags_cli(flags);
    old_value = value();
    if (old_value == test_value)
	CLICK_ATOMIC_VAL = new_value;
    __restore_flags(flags);
#else
    old_value = value();
    if (old_value == test_value)
	CLICK_ATOMIC_VAL = new_value;
#endif
    return old_value;
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
