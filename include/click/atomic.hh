#ifndef CLICK_ATOMIC_HH
#define CLICK_ATOMIC_HH

#if defined(__KERNEL__) && defined(__SMP__) /* && defined(__MTCLICK__) */

class u_atomic32_t { public:

  // No constructors because, unfortunately, they cause GCC to generate worse
  // code. Use operator= instead.
  
  operator u_int32_t() const		{ return atomic_read(&_val); }
  u_int32_t value() const		{ return atomic_read(&_val); }
  
  u_atomic32_t &operator=(u_int32_t u)	{ atomic_set(&_val, u); return *this; }
  u_atomic32_t &operator+=(int x)	{ atomic_add(x, &_val); return *this; }
  u_atomic32_t &operator-=(int x)	{ atomic_sub(x, &_val); return *this; }
  void operator++(int)			{ atomic_inc(&_val); }
  void operator--(int)			{ atomic_dec(&_val); }

  // returns true if value is 0 after decrement
  bool dec_and_test()			{ return atomic_dec_and_test(&_val); }

  // returns true if value is positive after increment
  bool inc_and_test_greater_zero()	{ return atomic_inc_and_test_greater_zero(&_val); }
  
  u_int32_t read_and_add(int x);
  u_int32_t compare_and_swap(u_int32_t old_value, u_int32_t new_value);
  
 private:

  atomic_t _val;

};

inline u_int32_t
u_atomic32_t::read_and_add(int x)
{
#ifdef __i386__
  asm volatile ("lock\n" "\txaddl %0,%1\n"
                : "=r" (x), "=m" (__atomic_fool_gcc(&_val)) 
		: "0" (x), "m" (__atomic_fool_gcc(&_val)));
#else
  StaticAssert("no mt click support for non i386 machines");
#endif
  return x;
}

inline u_int32_t
u_atomic32_t::compare_and_swap(u_int32_t old_value, u_int32_t new_value)
{
  int result;
  asm ("lock\n"
       "\tcmpxchgl %2, %0\n"
       : "=m" (__atomic_fool_gcc(&_val)), "=a" (result)
       : "r" (new_value), "m" (__atomic_fool_gcc(&_val)), "a" (old_value)
       // : "eax", "cc", "memory");
       : "cc", "memory");
  // return old value: compare and swap fails if old value is different from
  // val, succeeds otherwise.
  return result;
}

#else

class u_atomic32_t { public:
  
  // No constructors because, unfortunately, they cause GCC to generate worse
  // code. Use operator= instead.
  
  operator u_int32_t() const		{ return _val; }
  u_int32_t value() const		{ return _val; }

  u_atomic32_t &operator=(u_int32_t u)	{ _val = u; return *this; }
  u_atomic32_t &operator+=(int x)	{ _val += x; return *this; }
  u_atomic32_t &operator-=(int x)	{ _val -= x; return *this; }
  void operator++(int)			{ _val++; }
  void operator--(int)			{ _val--; }
  
  u_int32_t read_and_add(int x);
  u_int32_t compare_and_swap(u_int32_t old_value, u_int32_t new_value);
 
  // returns true if value is 0 after decrement
  bool dec_and_test()			{ _val--; return _val == 0; }

  // returns true if value is positive after increment
  bool inc_and_test_greater_zero()	{ _val++; return _val > 0; }
  
 private:

  u_int32_t _val;
  
};

inline u_int32_t
u_atomic32_t::read_and_add(int x)
{
  u_int32_t ov = _val;
  _val += x;
  return ov;
}

inline u_int32_t
u_atomic32_t::compare_and_swap(u_int32_t old_value, u_int32_t new_value)
{
  u_int32_t ov = _val;
  if (_val == old_value) _val = new_value;
  return ov;
}

#endif

inline u_int32_t
operator+(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() + b.value();
}

inline u_int32_t
operator-(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() - b.value();
}

inline bool
operator==(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() == b.value();
}

inline bool
operator!=(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() != b.value();
}

inline bool
operator>(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() > b.value();
}

inline bool
operator<(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() < b.value();
}

inline bool
operator>=(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() >= b.value();
}

inline bool
operator<=(const u_atomic32_t &a, const u_atomic32_t &b)
{
  return a.value() <= b.value();
}

#endif
