// -*- c-basic-offset: 2; related-file-name: "../include/click/string.hh" -*-
/*
 * string.{cc,hh} -- a String class with shared substrings
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/string.hh>
#include <click/straccum.hh>
#include <click/glue.hh>
#include <assert.h>

String::Memo *String::null_memo = 0;
String::Memo *String::permanent_memo = 0;
String::Memo *String::oom_memo = 0;
String *String::null_string_p = 0;
static int out_of_memory_flag = 0;

inline
String::Memo::Memo()
  : _refcount(0), _capacity(0), _dirty(0), _real_data("")
{
}

inline
String::Memo::Memo(char *data, int dirty, int capacity)
  : _refcount(0), _capacity(capacity), _dirty(dirty),
    _real_data(data)
{
}

String::Memo::Memo(int dirty, int capacity)
  : _refcount(1), _capacity(capacity), _dirty(dirty),
    _real_data(new char[capacity])
{
  assert(_capacity >= _dirty);
}

String::Memo::~Memo()
{
  if (_capacity) {
    assert(_capacity >= _dirty);
    delete[] _real_data;
  }
}


String::String(int i)
{
  char buf[128];
  sprintf(buf, "%d", i);
  assign(buf, -1);
}

String::String(unsigned u)
{
  char buf[128];
  sprintf(buf, "%u", u);
  assign(buf, -1);
}

String::String(long d)
{
  char buf[128];
  sprintf(buf, "%ld", d);
  assign(buf, -1);
}

String::String(unsigned long u)
{
  char buf[128];
  sprintf(buf, "%lu", u);
  assign(buf, -1);
}

#ifdef HAVE_INT64_TYPES
// Implemented a lovely [unsigned] long long converter in StringAccum
// (use the code even at user level to hunt out bugs)

String::String(int64_t q)
{
  StringAccum sa;
  sa << q;
  assign(sa.data(), sa.length());
}

String::String(uint64_t q)
{
  StringAccum sa;
  sa << q;
  assign(sa.data(), sa.length());
}

#endif

#ifdef CLICK_USERLEVEL

String::String(double d)
{
  char buf[128];
  sprintf(buf, "%f", d);
  assign(buf, -1);
}

#endif

String
String::claim_string(char *str, int len, int capacity)
{
  assert(str && len > 0 && capacity >= len);
  Memo *new_memo = new Memo(str, len, capacity);
  if (new_memo)
    return String(str, len, new_memo);
  else
    return String(oom_memo->_real_data, 0, oom_memo);
}

String
String::stable_string(const char *str, int len)
{
  if (len < 0)
    len = (str ? strlen(str) : 0);
  if (len == 0)
    return String();
  else
    return String(str, len, permanent_memo);
}

String
String::garbage_string(int len)
{
  String s;
  s.append_garbage(len);
  return s;
}

void
String::make_out_of_memory()
{
  if (_memo)
    deref();
  _memo = oom_memo;
  _memo->_refcount++;
  _data = _memo->_real_data;
  _length = 0;
  out_of_memory_flag++;
}

int
String::out_of_memory_count()
{
  return out_of_memory_flag;
}

void
String::assign(const char *str, int len)
{
  if (!str) {
    assert(len <= 0);
    len = 0;
  } else if (len < 0)
    len = strlen(str);
  
  if (len == 0) {
    _memo = (str == oom_memo->_real_data ? oom_memo : null_memo);
    _memo->_refcount++;
    
  } else {
    // Make `capacity' a multiple of 16 characters at least as big as `len'.
    int capacity = (len + 16) & ~15;
    _memo = new Memo(len, capacity);
    if (!_memo || !_memo->_real_data) {
      make_out_of_memory();
      return;
    }
    memcpy(_memo->_real_data, str, len);
  }
  
  _data = _memo->_real_data;
  _length = len;
}

void
String::append(const char *suffix, int suffix_len)
{
  if (!suffix) {
    assert(suffix_len <= 0);
    suffix_len = 0;
  } else if (suffix_len < 0)
    suffix_len = strlen(suffix);

  // Appending "out of memory" to a regular string makes it "out of memory",
  // and appending anything to "out of memory" leaves it as "out of memory"
  if (suffix_len == 0 || _memo == oom_memo) {
    if (suffix == oom_memo->_real_data)
      make_out_of_memory();
    return;
  }
  
  // If we can, append into unused space. First, we check that there's enough
  // unused space for `suffix_len' characters to fit; then, we check that the
  // unused space immediately follows the data in `*this'.
  if (_memo->_capacity > _memo->_dirty + suffix_len) {
    char *real_dirty = _memo->_real_data + _memo->_dirty;
    if (real_dirty == _data + _length) {
      memcpy(real_dirty, suffix, suffix_len);
      _length += suffix_len;
      _memo->_dirty += suffix_len;
      assert(_memo->_dirty < _memo->_capacity);
      return;
    }
  }
  
  // Now we have to make new space. Make sure the new capacity is a
  // multiple of 16 characters and that it is at least 16.
  int new_capacity = (_length + 16) & ~15;
  while (new_capacity < _length + suffix_len)
    new_capacity *= 2;
  Memo *new_memo = new Memo(_length + suffix_len, new_capacity);
  if (!new_memo || !new_memo->_real_data) {
    delete new_memo;
    make_out_of_memory();
    return;
  }

  char *new_data = new_memo->_real_data;
  memcpy(new_data, _data, _length);
  memcpy(new_data + _length, suffix, suffix_len);
  
  deref();
  _data = new_data;
  _length += suffix_len;
  _memo = new_memo;
}

void
String::append_fill(int c, int suffix_len)
{
  assert(suffix_len >= 0);
  if (suffix_len == 0 || _memo == oom_memo)
    return;
  
  // If we can, append into unused space. First, we check that there's enough
  // unused space for `suffix_len' characters to fit; then, we check that the
  // unused space immediately follows the data in `*this'.
  if (_memo->_capacity > _memo->_dirty + suffix_len) {
    char *real_dirty = _memo->_real_data + _memo->_dirty;
    if (real_dirty == _data + _length) {
      memset(real_dirty, c, suffix_len);
      _length += suffix_len;
      _memo->_dirty += suffix_len;
      assert(_memo->_dirty < _memo->_capacity);
      return;
    }
  }
  
  // Now we have to make new space. Make sure the new capacity is a
  // multiple of 16 characters and that it is at least 16.
  int new_capacity = (_length + 16) & ~15;
  while (new_capacity < _length + suffix_len)
    new_capacity *= 2;
  Memo *new_memo = new Memo(_length + suffix_len, new_capacity);
  if (!new_memo || !new_memo->_real_data) {
    delete new_memo;
    make_out_of_memory();
    return;
  }

  char *new_data = new_memo->_real_data;
  memcpy(new_data, _data, _length);
  memset(new_data + _length, c, suffix_len);
  
  deref();
  _data = new_data;
  _length += suffix_len;
  _memo = new_memo;
}

void
String::append_garbage(int suffix_len)
{
  assert(suffix_len >= 0);
  if (suffix_len == 0 || _memo == oom_memo)
    return;
  
  // If we can, append into unused space. First, we check that there's enough
  // unused space for `suffix_len' characters to fit; then, we check that the
  // unused space immediately follows the data in `*this'.
  if (_memo->_capacity > _memo->_dirty + suffix_len) {
    char *real_dirty = _memo->_real_data + _memo->_dirty;
    if (real_dirty == _data + _length) {
      _length += suffix_len;
      _memo->_dirty += suffix_len;
      assert(_memo->_dirty < _memo->_capacity);
      return;
    }
  }
  
  // Now we have to make new space. Make sure the new capacity is a
  // multiple of 16 characters and that it is at least 16.
  int new_capacity = (_length + 16) & ~15;
  while (new_capacity < _length + suffix_len)
    new_capacity *= 2;
  Memo *new_memo = new Memo(_length + suffix_len, new_capacity);
  if (!new_memo || !new_memo->_real_data) {
    delete new_memo;
    out_of_memory();
    return;
  }

  char *new_data = new_memo->_real_data;
  memcpy(new_data, _data, _length);
  
  deref();
  _data = new_data;
  _length += suffix_len;
  _memo = new_memo;
}

char *
String::mutable_data()
{
  // If _memo has a capacity (it's not one of the special strings) and it's
  // uniquely referenced, return _data right away.
  if (_memo->_capacity && _memo->_refcount == 1)
    return const_cast<char *>(_data);
  
  // Otherwise, make a copy of it. Rely on: deref() doesn't change _data or
  // _length; and if _capacity == 0, then deref() doesn't free _real_data.
  assert(!_memo->_capacity || _memo->_refcount > 1);
  deref();
  assign(_data, _length);
  return const_cast<char *>(_data);
}

char *
String::mutable_c_str()
{
  (void) mutable_data();
  (void) cc();
  return const_cast<char *>(_data);
}

const char *
String::cc()
{
  // If _memo has no capacity, then this is one of the special strings (null
  // or PermString). We are guaranteed, in these strings, that _data[_length]
  // exists. We can return _data immediately if we have a '\0' in the right
  // place.
  if (!_memo->_capacity && _data[_length] == '\0')
    return _data;
  
  // Otherwise, this invariant must hold (there's more real data in _memo than
  // in our substring).
  assert(!_memo->_capacity
	 || _memo->_real_data + _memo->_dirty >= _data + _length);
  
  // Has the character after our substring been set?
  if (_memo->_real_data + _memo->_dirty == _data + _length) {
    // Character after our substring has not been set. May be able to change
    // it to '\0'. This case will never occur on special strings.
    if (_memo->_dirty < _memo->_capacity)
      goto add_final_nul;
    
  } else {
    // Character after our substring has been set. OK to return _data if it is
    // already '\0'.
    if (_data[_length] == '\0')
      return _data;
  }
  
  // If we get here, we must make a copy of our portion of the string.
  {
    String s(_data, _length);
    deref();
    assign(s);
  }
  
 add_final_nul:
  char *real_data = const_cast<char *>(_data);
  real_data[_length] = '\0';
  _memo->_dirty++;		// include '\0' in used portion of _memo
  return _data;
}

String
String::substring(int left, int len) const
{
  if (left < 0)
    left += _length;
  if (len < 0)
    len = _length - left + len;
  if (left + len > _length)
    len = _length - left;
  
  if (left < 0 || len <= 0)
    return String();
  else
    return String(_data + left, len, _memo);
}

int
String::find_left(int c, int start) const
{
  if (start < 0) start = 0;
  for (int i = start; i < _length; i++)
    if ((unsigned char)_data[i] == c)
      return i;
  return -1;
}

int
String::find_left(const String &s, int start) const
{
  if (start < 0) start = 0;
  if (start >= length()) return -1;
  if (!s.length()) return 0;
  int first_c = (unsigned char)s[0];
  int pos = start, max_pos = length() - s.length();
  for (pos = find_left(first_c, pos); pos >= 0 && pos <= max_pos;
       pos = find_left(first_c, pos + 1))
    if (!memcmp(_data + pos, s._data, s.length()))
      return pos;
  return -1;
}

int
String::find_right(int c, int start) const
{
  if (start >= _length) start = _length - 1;
  for (int i = start; i >= 0; i--)
    if ((unsigned char)_data[i] == c)
      return i;
  return -1;
}

static String
hard_lower(const String &s, int pos)
{
  String new_s(s.data(), s.length());
  char *x = const_cast<char *>(new_s.data()); // know it's mutable
  int len = s.length();
  for (; pos < len; pos++)
    x[pos] = tolower(x[pos]);
  return new_s;
}

String
String::lower() const
{
  // avoid copies
  for (int i = 0; i < _length; i++)
    if (_data[i] >= 'A' && _data[i] <= 'Z')
      return hard_lower(*this, i);
  return *this;
}

static String
hard_upper(const String &s, int pos)
{
  String new_s(s.data(), s.length());
  char *x = const_cast<char *>(new_s.data()); // know it's mutable
  int len = s.length();
  for (; pos < len; pos++)
    x[pos] = toupper(x[pos]);
  return new_s;
}

String
String::upper() const
{
  // avoid copies
  for (int i = 0; i < _length; i++)
    if (_data[i] >= 'a' && _data[i] <= 'z')
      return hard_upper(*this, i);
  return *this;
}

static String
hard_printable(const String &s, int pos)
{
  StringAccum sa(s.length() * 2);
  sa.append(s.data(), pos);
  const unsigned char *x = reinterpret_cast<const unsigned char *>(s.data());
  int len = s.length();
  for (; pos < len; pos++) {
    if (x[pos] >= 32 && x[pos] < 127)
      sa << x[pos];
    else if (x[pos] < 32)
      sa << '^' << (unsigned char)(x[pos] + 64);
    else if (char *buf = sa.extend(4, 1))
      sprintf(buf, "\\%03o", x[pos]);
  }
  return sa.take_string();
}

String
String::printable() const
{
  // avoid copies
  for (int i = 0; i < _length; i++)
    if (_data[i] < 32 || _data[i] > 126)
      return hard_printable(*this, i);
  return *this;
}

int
hashcode(const String &s)
{
  int length = s.length();
  const char *data = s.data();
  if (!length)
    return 0;
  else if (length == 1)
    return data[0] | (data[0] << 8);
  else if (length < 4)
    return data[0] + (data[1] << 3) + (length << 12);
  else
    return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24)
      + (length << 12) + (data[length-1] << 10);
}

bool
String::equals(const char *s, int len) const
{
  if (len < 0)
    len = strlen(s);
  // "out-of-memory" strings compare unequal to anything, even themselves
  if (_length != len || s == oom_memo->_real_data || _memo == oom_memo)
    return false;
  if (_data == s)
    return true;
  return memcmp(_data, s, len) == 0;
}

int
String::compare(const char *s, int len) const
{
  if (len < 0)
    len = strlen(s);
  if (_data == s && _length == len)
    return 0;
  if (_length == len)
    return memcmp(_data, s, len);
  else if (_length < len) {
    int v = memcmp(_data, s, _length);
    return (v ? v : -1);
  } else {
    int v = memcmp(_data, s, len);
    return (v ? v : 1);
  }
}


String::Initializer::Initializer()
{
  String::static_initialize();
}

void
String::static_initialize()
{
  // function called to initialize static globals
  if (!null_memo) {
    null_memo = new Memo;
    null_memo->_refcount++;
    permanent_memo = new Memo;
    permanent_memo->_refcount++;
    oom_memo = new Memo(0, 1);
    oom_memo->_real_data[0] = '\0';
    null_string_p = new String;
  }
}

void
String::static_cleanup()
{
  if (null_string_p) {
    delete null_string_p;
    null_string_p = 0;
    if (--null_memo->_refcount == 0)
      delete null_memo;
    if (--permanent_memo->_refcount == 0)
      delete permanent_memo;
    if (--oom_memo->_refcount == 0)
      delete oom_memo;
    null_memo = permanent_memo = oom_memo = 0;
  }
}
