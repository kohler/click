// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ATOMIC_HH
#define CLICK_ATOMIC_HH
CLICK_DECLS

#ifdef __KERNEL__
# ifdef __SMP__
#  define LOCK "lock ; "
# else
#  define LOCK ""
# endif

class atomic_uint32_t { public:

    // No constructors because, unfortunately, GCC generates worse code. Use
    // operator= instead.
  
    operator uint32_t() const		{ return atomic_read(&_val); }
    uint32_t value() const		{ return atomic_read(&_val); }
  
    atomic_uint32_t &operator=(uint32_t u) { atomic_set(&_val, u); return *this;}

    atomic_uint32_t &operator+=(int x)	{ atomic_add(x, &_val); return *this; }
    atomic_uint32_t &operator-=(int x)	{ atomic_sub(x, &_val); return *this; }
    inline atomic_uint32_t &operator|=(uint32_t);
    inline atomic_uint32_t &operator&=(uint32_t);

    void operator++(int)		{ atomic_inc(&_val); }
    void operator--(int)		{ atomic_dec(&_val); }

    // returns true if value is 0 after decrement
    bool dec_and_test()			{ return atomic_dec_and_test(&_val); }

    inline uint32_t read_and_add(uint32_t delta);
    inline uint32_t compare_and_swap(uint32_t test, uint32_t new_value);
  
  private:

    atomic_t _val;

};

inline atomic_uint32_t &
atomic_uint32_t::operator|=(uint32_t u)
{
# ifdef __arm__
    unsigned long flags;
    __save_flags_cli(flags);
    _val.counter |= u;
    __restore_flags(flags);
# else
    atomic_set_mask(u, &_val);
# endif
    return *this;
}

inline atomic_uint32_t &
atomic_uint32_t::operator&=(uint32_t u)
{
# ifdef __arm__
    unsigned long flags;
    __save_flags_cli(flags);
    _val.counter &= u;
    __restore_flags(flags);
# else
    atomic_clear_mask(~u, &_val);
# endif
    return *this;
}

inline uint32_t
atomic_uint32_t::read_and_add(uint32_t delta)
{
# if defined(__i386__) || defined(__arch_um__)
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    asm volatile (LOCK "xaddl %0,%1"
		  : "=r" (delta), "=m" (_val.counter) 
		  : "0" (delta), "m" (_val.counter));
#  else
    asm volatile (LOCK "xaddl %0,%1"
		  : "=r" (delta), "=m" (__atomic_fool_gcc(&_val)) 
		  : "0" (delta), "m" (__atomic_fool_gcc(&_val)));
#  endif
    return delta;
# else
    unsigned long flags;
    __save_flags_cli(flags);
    uint32_t old_value = value();
    *this += delta;
    __restore_flags(flags);
    return old_value;
# endif
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
# if defined(__i386__) || defined(__arch_um__)
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    asm (LOCK "cmpxchgl %2,%0"
	 : "=m" (_val.counter), "=a" (old_value)
	 : "r" (new_value), "m" (_val.counter), "a" (test_value)
	 // : "eax", "cc", "memory");
	 : "cc", "memory");
#  else
    asm (LOCK "cmpxchgl %2,%0"
	 : "=m" (__atomic_fool_gcc(&_val)), "=a" (old_value)
	 : "r" (new_value), "m" (__atomic_fool_gcc(&_val)), "a" (test_value)
	 // : "eax", "cc", "memory");
	 : "cc", "memory");
#  endif
# else
    unsigned long flags;
    __save_flags_cli(flags);
    old_value = value();
    if (old_value == test_value)
	*this = new_value;
    __restore_flags(flags);
# endif
    return old_value;
}


# undef LOCK
#else /* !__KERNEL__ */


class atomic_uint32_t { public:
  
    // No constructors because, unfortunately, GCC generates worse code. Use
    // operator= instead.
  
    operator uint32_t() const		{ return _val; }
    uint32_t value() const		{ return _val; }

    atomic_uint32_t &operator=(uint32_t u) { _val = u; return *this; }
  
    atomic_uint32_t &operator+=(int x)	{ _val += x; return *this; }
    atomic_uint32_t &operator-=(int x)	{ _val -= x; return *this; }
    atomic_uint32_t &operator&=(uint32_t u) { _val &= u; return *this; }
    atomic_uint32_t &operator|=(uint32_t u) { _val |= u; return *this; }
  
    void operator++(int)		{ _val++; }
    void operator--(int)		{ _val--; }

    // returns true if value is 0 after decrement
    bool dec_and_test()			{ _val--; return _val == 0; }

    inline uint32_t read_and_add(uint32_t delta);
    inline uint32_t compare_and_swap(uint32_t test_value, uint32_t new_value);

  private:

    volatile uint32_t _val;
  
};

inline uint32_t
atomic_uint32_t::read_and_add(uint32_t delta)
{
    uint32_t old_value = _val;
    _val += delta;
    return old_value;
}

inline uint32_t
atomic_uint32_t::compare_and_swap(uint32_t test_value, uint32_t new_value)
{
    uint32_t old_value = _val;
    if (_val == test_value)
	_val = new_value;
    return old_value;
}

#endif /* __KERNEL__ */

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
