#ifndef CLICK_ATOMIC_HH
#define CLICK_ATOMIC_HH
CLICK_DECLS

#if defined(__KERNEL__) && defined(__SMP__) /* && defined(__MTCLICK__) */

class uatomic32_t { public:

  // No constructors because, unfortunately, they cause GCC to generate worse
  // code. Use operator= instead.
  
  operator uint32_t() const		{ return atomic_read(&_val); }
  uint32_t value() const		{ return atomic_read(&_val); }
  
  uatomic32_t &operator=(uint32_t u)	{ atomic_set(&_val, u); return *this; }
  uatomic32_t &operator+=(int x)	{ atomic_add(x, &_val); return *this; }
  uatomic32_t &operator-=(int x)	{ atomic_sub(x, &_val); return *this; }
  void operator++(int)			{ atomic_inc(&_val); }
  void operator--(int)			{ atomic_dec(&_val); }

  // returns true if value is 0 after decrement
  bool dec_and_test()			{ return atomic_dec_and_test(&_val); }

  uint32_t read_and_add(int x);
  uint32_t compare_and_swap(uint32_t old_value, uint32_t new_value);
  
 private:

  atomic_t _val;

};

inline uint32_t
uatomic32_t::read_and_add(int x)
{
#ifdef __i386__
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
  asm volatile ("lock\n" "\txaddl %0,%1\n"
                : "=r" (x), "=m" (_val.counter) 
		: "0" (x), "m" (_val.counter));
# else
  asm volatile ("lock\n" "\txaddl %0,%1\n"
                : "=r" (x), "=m" (__atomic_fool_gcc(&_val)) 
		: "0" (x), "m" (__atomic_fool_gcc(&_val)));
# endif
#else
  StaticAssert("no support for SMP on non-i386 machines");
#endif
  return x;
}

inline uint32_t
uatomic32_t::compare_and_swap(uint32_t old_value, uint32_t new_value)
{
#ifdef __i386__
  int result;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
  asm ("lock\n"
       "\tcmpxchgl %2, %0\n"
       : "=m" (_val.counter), "=a" (result)
       : "r" (new_value), "m" (_val.counter), "a" (old_value)
       // : "eax", "cc", "memory");
       : "cc", "memory");
# else
  asm ("lock\n"
       "\tcmpxchgl %2, %0\n"
       : "=m" (__atomic_fool_gcc(&_val)), "=a" (result)
       : "r" (new_value), "m" (__atomic_fool_gcc(&_val)), "a" (old_value)
       // : "eax", "cc", "memory");
       : "cc", "memory");
# endif
  // return old value: compare and swap fails if old value is different from
  // val, succeeds otherwise.
  return result;
#else
  StaticAssert("no support for SMP on non-i386 machines");
  return 0;
#endif
}

#else

class uatomic32_t { public:
  
  // No constructors because, unfortunately, they cause GCC to generate worse
  // code. Use operator= instead.
  
  operator uint32_t() const		{ return _val; }
  uint32_t value() const		{ return _val; }

  uatomic32_t &operator=(uint32_t u)	{ _val = u; return *this; }
  uatomic32_t &operator+=(int x)	{ _val += x; return *this; }
  uatomic32_t &operator-=(int x)	{ _val -= x; return *this; }
  void operator++(int)			{ _val++; }
  void operator--(int)			{ _val--; }
  
  uint32_t read_and_add(int x);
  uint32_t compare_and_swap(uint32_t old_value, uint32_t new_value);
 
  // returns true if value is 0 after decrement
  bool dec_and_test()			{ _val--; return _val == 0; }

 private:

  uint32_t _val;
  
};

inline uint32_t
uatomic32_t::read_and_add(int x)
{
  uint32_t ov = _val;
  _val += x;
  return ov;
}

inline uint32_t
uatomic32_t::compare_and_swap(uint32_t old_value, uint32_t new_value)
{
  uint32_t ov = _val;
  if (_val == old_value) _val = new_value;
  return ov;
}

#endif

inline uint32_t
operator+(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() + b.value();
}

inline uint32_t
operator-(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() - b.value();
}

inline bool
operator==(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() == b.value();
}

inline bool
operator!=(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() != b.value();
}

inline bool
operator>(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() > b.value();
}

inline bool
operator<(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() < b.value();
}

inline bool
operator>=(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() >= b.value();
}

inline bool
operator<=(const uatomic32_t &a, const uatomic32_t &b)
{
  return a.value() <= b.value();
}

CLICK_ENDDECLS
#endif
