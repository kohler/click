// -*- c-basic-offset: 4 -*-
/*
 * json.{cc,hh} -- Json helper class
 * Eddie Kohler
 *
 * Copyright (c) 2009-2012 Meraki, Inc.
 * Copyright (c) 2009-2012 Eddie Kohler
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
#include "json.hh"
#include <click/confparse.hh>
CLICK_DECLS

/** @class Json
    @brief Json data.

    The Json class represents Json data: null values, booleans, numbers,
    strings, and combinations of these primitives into arrays and objects.

    Json objects are not references, and two Json values cannot share
    subobjects. This differs from Javascript. For example:

    <code>
    Json j1 = Json::make_object(), j2 = Json::make_object();
    j1.set("a", j2); // stores a COPY of j2 in j1
    j2.set("b", 1);
    assert(j1.unparse() == "{\"a\":{}}");
    assert(j2.unparse() == "{\"b\":1}");
    </code>

    Compare this with the Javascript code:

    <code>
    var j1 = {}, j2 = {};
    j1.a = j2; // stores a REFERENCE to j2 in j1
    j2.b = 1;
    assert(JSON.stringify(j1) == "{\"a\":{\"b\":1}}");
    </code>

    Most Json functions for extracting components and typed values behave
    liberally. For example, objects silently convert to integers, and
    extracting properties from non-objects is allowed. This should make it
    easier to work with untrusted objects. (Json objects often originate
    from untrusted sources, and may not have the types you expect.) If you
    prefer an assertion to fail when a Json object has an unexpected type,
    use the checked <tt>as_</tt> and <code>at</code> functions, rather than
    the liberal <tt>to_</tt>, <tt>get</tt>, and <tt>operator[]</code>
    functions. */

char Json::statics[sizeof(JsonStatics)];

inline Json::JsonStatics::JsonStatics()
    : null_json(), array_string(String::make_stable("[Array]", 7)),
      object_string(String::make_stable("[Object]", 8)) {
}

void Json::static_initialize() {
    new((void *) statics) JsonStatics;
}

// Object internals

Json::ObjectJson::ObjectJson(const ObjectJson &x)
    : ComplexJson(), os_(x.os_), n_(x.n_), capacity_(x.capacity_),
      hash_(x.hash_), nremoved_(x.nremoved_)
{
    grow(true);
}

Json::ObjectJson::~ObjectJson()
{
    ObjectItem *ob = os_, *oe = ob + n_;
    for (; ob != oe; ++ob)
	if (ob->next_ > -2)
	    ob->~ObjectItem();
    delete[] reinterpret_cast<char *>(os_);
}

void Json::ObjectJson::grow(bool copy)
{
    if (copy && !capacity_)
	return;
    int new_capacity;
    if (copy)
	new_capacity = capacity_;
    else if (capacity_)
	new_capacity = capacity_ * 2;
    else
	new_capacity = 8;
    ObjectItem *new_os = reinterpret_cast<ObjectItem *>(operator new[](sizeof(ObjectItem) * new_capacity));
    ObjectItem *ob = os_, *oe = ob + n_;
    for (ObjectItem *oi = new_os; ob != oe; ++oi, ++ob)
	if (ob->next_ > -2) {
	    new((void *) oi) ObjectItem(ob->v_.first, ob->v_.second, ob->next_);
	    if (!copy)
		ob->~ObjectItem();
	} else
	    oi->next_ = -2;
    if (!copy)
	operator delete[](reinterpret_cast<void *>(os_));
    os_ = new_os;
    capacity_ = new_capacity;
}

void Json::ObjectJson::rehash()
{
    hash_.assign(hash_.size() * 2, -1);
    for (int i = n_ - 1; i >= 0; --i) {
	ObjectItem &oi = item(i);
	if (oi.next_ > -2) {
	    int b = bucket(oi.v_.first.data(), oi.v_.first.length());
	    oi.next_ = hash_[b];
	    hash_[b] = i;
	}
    }
}

int Json::ObjectJson::find_insert(const String &key, const Json &value)
{
    if (hash_.empty())
	hash_.assign(8, -1);
    int *b = &hash_[bucket(key.data(), key.length())], chain = 0;
    while (*b >= 0 && os_[*b].v_.first != key) {
	b = &os_[*b].next_;
	++chain;
    }
    if (*b >= 0)
	return *b;
    else {
	*b = n_;
	if (n_ == capacity_)
	    grow(false);
	// NB 'b' is invalid now
	new ((void *) &os_[n_]) ObjectItem(key, value, -1);
	++n_;
	if (chain > 4)
	    rehash();
	return n_ - 1;
    }
}

Json &Json::ObjectJson::get_insert(const StringRef &key)
{
    if (hash_.empty())
	hash_.assign(8, -1);
    int *b = &hash_[bucket(key.data(), key.length())], chain = 0;
    while (*b >= 0 && !os_[*b].v_.first.equals(key.data(), key.length())) {
	b = &os_[*b].next_;
	++chain;
    }
    if (*b >= 0)
	return os_[*b].v_.second;
    else {
	*b = n_;
	if (n_ == capacity_)
	    grow(false);
	// NB 'b' is invalid now
	new ((void *) &os_[n_]) ObjectItem(String(key.data(), key.length()), make_null(), -1);
	++n_;
	if (chain > 4)
	    rehash();
	return os_[n_ - 1].v_.second;
    }
}

Json::size_type Json::ObjectJson::erase(const StringRef &key)
{
    int *b = &hash_[bucket(key.data(), key.length())];
    while (*b >= 0 && !os_[*b].v_.first.equals(key.data(), key.length()))
	b = &os_[*b].next_;
    if (*b >= 0) {
	int p = *b;
	*b = os_[p].next_;
	os_[p].~ObjectItem();
	os_[p].next_ = -2;
	++nremoved_;
	return 1;
    } else
	return 0;
}

namespace {
template <typename T> bool string_to_int_key(const char *first,
					     const char *last, T &x)
{
    if (first == last || !isdigit((unsigned char) *first)
	|| (first[0] == '0' && first + 1 != last && first[1] == '0'))
	return false;
    x = *first - '0';
    for (++first; first != last && isdigit((unsigned char) *first); ++first)
	x = 10 * x + *first - '0';
    return first == last;
}
}

void Json::hard_uniqueify(int type)
{
    if (type < 0) {
	assert(_type == j_null || _type == -type);
	type = -type;
    }

    ComplexJson *old_cjson = _cjson;
    if (old_cjson && type == _type && type == j_object)
	_cjson = new ObjectJson(*static_cast<ObjectJson *>(old_cjson));
    else if (old_cjson && type == _type && type == j_array)
	_cjson = new ArrayJson(*static_cast<ArrayJson *>(old_cjson));
    else if (type == j_object) {
	_cjson = new ObjectJson;
	if (old_cjson) {
	    ArrayJson *old_ajson = static_cast<ArrayJson *>(old_cjson);
	    ObjectJson *ojson = static_cast<ObjectJson *>(_cjson);
	    for (JsonVector::size_type i = 0;
		 i != old_ajson->values.size();
		 ++i)
		ojson->find_insert(String(i), old_ajson->values[i]);
	}
    } else {
	_cjson = new ArrayJson;
	if (old_cjson) {
	    ObjectJson *old_ojson = static_cast<ObjectJson *>(old_cjson);
	    ArrayJson *ajson = static_cast<ArrayJson *>(_cjson);
	    ObjectItem *ob = old_ojson->os_, *oe = ob + old_ojson->n_;
	    JsonVector::size_type i;
	    for (; ob != oe; ++ob)
		if (ob->next_ > -2
		    && string_to_int_key(ob->v_.first.begin(),
					 ob->v_.first.end(), i)) {
		    if (i < ajson->values.size())
			ajson->values.resize(i + 1);
		    ajson->values[i] = ob->v_.second;
		}
	}
    }

    if (old_cjson)
	old_cjson->deref(_type);
    _type = (json_type) type;
}


// Primitives

long Json::hard_to_i() const
{
    switch (_type) {
    case j_null:
	return 0;
    case j_array:
    case j_object:
	return size();
    case j_bool:
	return _str[0] == 't';
    case j_int:
    case j_double:
    case j_string:
    default:
	const char *b = _str.c_str();
	char *s;
	long x = strtol(b, &s, 0);
#if HAVE_FLOAT_TYPES
	if (s == b + _str.length())
	    return x;
	else
	    return (long) strtod(b, 0);
#else
	return x;
#endif
    }
}

long Json::hard_as_i() const
{
    const char *b = _str.c_str();
    char *s;
    long x = strtol(b, &s, 0);
    assert(s == b + _str.length());
    return x;
}

uint64_t Json::hard_to_u64() const
{
    switch (_type) {
    case j_null:
	return 0;
    case j_array:
    case j_object:
	return size();
    case j_bool:
	return _str[0] == 't';
    case j_int:
    case j_double:
    case j_string:
    default:
#if SIZEOF_LONG >= 8 || !HAVE_LONG_LONG
	unsigned long x;
#else
	unsigned long long x;
#endif
#if HAVE_FLOAT_TYPES
	if (cp_integer(_str.begin(), _str.end(), 10, &x) == _str.end())
	    return x;
	else
	    return (uint64_t) strtod(_str.c_str(), 0);
#else
	cp_integer(_str.begin(), _str.end(), 10, &x);
	return x;
#endif
    }
}

/** @brief Extract this integer Json's value into @a x.
    @param[out] x value storage
    @return True iff this Json stores an integer value.

    If false is returned (!is_number() or the number is not parseable as a
    pure integer), @a x remains unchanged. */
bool Json::to_i(int &x) const
{
    return (_type == j_int || _type == j_double)
	&& cp_integer(_str.begin(), _str.end(), 10, &x) == _str.end();
}

/** @overload */
bool Json::to_i(unsigned &x) const
{
    return (_type == j_int || _type == j_double)
	&& cp_integer(_str.begin(), _str.end(), 10, &x) == _str.end();
}

/** @overload */
bool Json::to_i(long &x) const
{
    return (_type == j_int || _type == j_double)
	&& cp_integer(_str.begin(), _str.end(), 10, &x) == _str.end();
}

/** @overload */
bool Json::to_i(unsigned long &x) const
{
    return (_type == j_int || _type == j_double)
	&& cp_integer(_str.begin(), _str.end(), 10, &x) == _str.end();
}

#if HAVE_LONG_LONG
/** @overload */
bool Json::to_i(long long &x) const
{
    return (_type == j_int || _type == j_double)
	&& cp_integer(_str.begin(), _str.end(), 10, &x) == _str.end();
}

/** @overload */
bool Json::to_i(unsigned long long &x) const
{
    return (_type == j_int || _type == j_double)
	&& cp_integer(_str.begin(), _str.end(), 10, &x) == _str.end();
}
#endif

#if HAVE_FLOAT_TYPES
/** @brief Return this Json converted to a double.

    Converts any Json to an integer value. Numeric Jsons convert as you'd
    expect. Null Jsons convert to 0; false boolean Jsons to 0 and true
    boolean Jsons to 1; string Jsons to a number parsed from their initial
    portions; and array and object Jsons to size().
    @sa as_d() */
double Json::to_d() const
{
    switch (_type) {
    case j_null:
	return 0;
    case j_array:
    case j_object:
	return size();
    case j_bool:
	return _str[0] == 't';
    case j_int:
    case j_double:
    case j_string:
    default:
	return strtod(_str.c_str(), 0);
    }
}
#endif

bool Json::hard_to_b() const
{
    switch (_type) {
    case j_null:
	return false;
    case j_array:
    case j_object:
	return !empty();
    case j_bool:
	return _str[0] == 't';
    case j_int:
	return to_i() != 0;
    case j_double:
#if HAVE_FLOAT_TYPES
	return to_d() != 0;
#else
	return !_str.equals("0");
#endif
    case j_string:
    default:
	return !_str.empty();
    }
}

const String &Json::hard_to_s() const
{
    switch (_type) {
    case j_null:
	return String::make_empty();
    case j_array:
	return reinterpret_cast<JsonStatics *>(statics)->array_string;
    case j_object:
	return reinterpret_cast<JsonStatics *>(statics)->object_string;
    case j_bool:
    case j_int:
    case j_double:
    case j_string:
    default:
	return _str;
    }
}

const Json &Json::hard_get(const StringRef &key) const
{
    ArrayJson *aj;
    JsonVector::size_type i;
    if (_type == j_array && (aj = ajson())
	&& string_to_int_key(key.begin(), key.end(), i)
	&& i < aj->values.size())
	return aj->values[i];
    else
	return make_null();
}

const Json &Json::hard_get(size_type x) const
{
    if (_type == j_object && _cjson)
	return get(String(x));
    else
	return make_null();
}

Json &Json::hard_get_insert(size_type x)
{
    if (_type == j_object)
	return get_insert(String(x));
    else {
	uniqueify_array(true);
	ArrayJson *aj = ajson();
	if (JsonVector::size_type(x) >= aj->values.size())
	    aj->values.resize(x + 1);
	return aj->values[x];
    }
}


// Unparsing

bool Json::unparse_is_complex() const
{
    if (_type == j_object) {
	if (ObjectJson *oj = ojson()) {
	    if (oj->n_ - oj->nremoved_ > 5)
		return true;
	    ObjectItem *ob = oj->os_, *oe = ob + oj->n_;
	    for (; ob != oe; ++ob)
		if (ob->next_ > -2 && !ob->v_.second.empty() && !ob->v_.second.is_primitive())
		    return true;
	}
    } else if (_type == j_array) {
	if (ArrayJson *aj = ajson()) {
	    if (aj->values.size() > 8)
		return true;
	    for (JsonVector::const_iterator it = aj->values.begin();
		 it != aj->values.end(); ++it)
		if (!it->empty() && !it->is_primitive())
		    return true;
	}
    }
    return false;
}

void Json::unparse_indent(StringAccum &sa, const unparse_manipulator &m, int depth)
{
    depth *= m.tab_width();
    sa.append_fill('\t', depth / 8);
    sa.append_fill(' ', depth % 8);
}

void Json::hard_unparse(StringAccum &sa, const unparse_manipulator &m, int depth) const
{
    if (depth >= m.indent_depth() || m.tab_width() == 0
	|| !unparse_is_complex()) {
	unparse(sa);
	return;
    }

    assert(_type == j_object || _type == j_array);
    if (_type == j_object) {
	const char *q = "{\n";
	ObjectJson *oj = ojson();
	ObjectItem *ob = oj->os_, *oe = ob + oj->n_;
	for (; ob != oe; ++ob)
	    if (ob->next_ > -2) {
		sa.append(q, 2);
		unparse_indent(sa, m, depth + 1);
		sa << '\"' << ob->v_.first.encode_json() << '\"' << ':';
		ob->v_.second.hard_unparse(sa, m, depth + 1);
		q = ",\n";
	    }
	sa << '\n';
	unparse_indent(sa, m, depth);
	sa << '}';
    } else if (_type == j_array) {
	const char *q = "[\n";
	ArrayJson *aj = ajson();
	for (JsonVector::const_iterator it = aj->values.begin();
	     it != aj->values.end(); ++it) {
	    sa.append(q, 2);
	    unparse_indent(sa, m, depth + 1);
	    it->hard_unparse(sa, m, depth + 1);
	    q = ",\n";
	}
	sa << '\n';
	unparse_indent(sa, m, depth);
	sa << ']';
    }
}

/** @brief Unparse the string representation of this Json into @a sa. */
void Json::unparse(StringAccum &sa) const
{
    if (_type == j_object) {
	char q = '{';
	if (ObjectJson *oj = ojson()) {
	    // Produces human-friendlier results if we output properties in
	    // order of insert.
	    ObjectItem *ob = oj->os_, *oe = ob + oj->n_;
	    for (; ob != oe; ++ob)
		if (ob->next_ > -2) {
		    sa << q << '\"' << ob->v_.first.encode_json() << '\"'
		       << ':' << ob->v_.second;
		    q = ',';
		}
	}
	if (q == '{')
	    sa << q;
	sa << '}';
    } else if (_type == j_array) {
	char q = '[';
	if (ArrayJson *aj = ajson())
	    for (JsonVector::const_iterator it = aj->values.begin();
		 it != aj->values.end(); ++it) {
		sa << q << *it;
		q = ',';
	    }
	if (q == '[')
	    sa << q;
	sa << ']';
    } else if (_type == j_string)
	sa << '\"' << _str.encode_json() << '\"';
    else if (_type == j_null)
	sa.append("null", 4);
    else {
	assert(_str);
	sa << _str;
    }
}


inline const char *
Json::skip_space(const char *s, const char *end)
{
    while (s != end && (unsigned char) *s <= 32
	   && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'))
	++s;
    return s;
}

bool
Json::assign_parse(const String &str, const char *s, const char *end)
{
    int state = st_initial;
    Json result;
    String key;
    Vector<Json *> stack;

    while (1) {
    next_token:
	s = skip_space(s, end);
	if (s == end)
	    return false;

	switch (*s) {

	case ',':
	    if (state == st_object_delim) {
		state = st_object_key;
		++s;
		goto next_token;
	    } else if (state == st_array_delim) {
		state = st_array_value;
		++s;
		goto next_token;
	    } else
		return false;

	case ':':
	    if (state == st_object_colon) {
		state = st_object_value;
		++s;
		goto next_token;
	    } else
		return false;

	case '}':
	    if (state == st_object_initial || state == st_object_delim) {
	    parse_complex_value:
		++s;
		stack.pop_back();
	    parse_value:
		if (stack.size() == 0)
		    goto done;
		state = (stack.back()->is_object() ? st_object_delim : st_array_delim);
		goto next_token;
	    } else
		return false;

	case ']':
	    if (state == st_array_initial || state == st_array_delim)
		goto parse_complex_value;
	    else
		return false;

	case '\"':
	    if (state == st_object_initial || state == st_object_key) {
		if ((s = parse_string(key, str, s + 1, end))) {
		    state = st_object_colon;
		    goto next_token;
		} else
		    return false;
	    }
	    break;

	}

	Json *this_value;
	if (state == st_initial)
	    this_value = &result;
	else if (state == st_object_value)
	    this_value = &stack.back()->get_insert(key);
	else if (state == st_array_initial || state == st_array_value) {
	    stack.back()->push_back(Json());
	    this_value = &stack.back()->back();
	} else
	    return false;

	switch (*s) {

	case '{':
	    if (stack.size() < max_depth) {
		*this_value = Json::make_object();
		state = st_object_initial;
		stack.push_back(this_value);
		++s;
		goto next_token;
	    } else
		return false;

	case '[':
	    if (stack.size() < max_depth) {
		*this_value = Json::make_array();
		state = st_array_initial;
		stack.push_back(this_value);
		++s;
		goto next_token;
	    } else
		return false;

	case '\"':
	    if ((s = parse_string(key, str, s + 1, end))) {
		*this_value = Json::make_string(key);
		goto parse_value;
	    } else
		return false;

	default:
	    if ((s = this_value->parse_primitive(str, s, end)))
		goto parse_value;
	    else
		return false;

	}
    }

 done:
    if (skip_space(s, end) == end) {
	swap(result);
	return true;
    } else
	return false;
}

const char *
Json::parse_string(String &result, const String &str, const char *s, const char *end)
{
    if (s == end)
	return 0;
    StringAccum sa;
    const char *last = s;
    for (; s != end; ++s) {
	if (*s == '\\') {
	    if (s + 1 == end)
		return 0;
	    sa.append(last, s);
	    if (s[1] == '\"' || s[1] == '\\' || s[1] == '/')
		++s, last = s;
	    else if (s[1] == 'b') {
		sa.append('\b');
		++s, last = s + 1;
	    } else if (s[1] == 'f') {
		sa.append('\f');
		++s, last = s + 1;
	    } else if (s[1] == 'n') {
		sa.append('\n');
		++s, last = s + 1;
	    } else if (s[1] == 'r') {
		sa.append('\r');
		++s, last = s + 1;
	    } else if (s[1] == 't') {
		sa.append('\t');
		++s, last = s + 1;
	    } else if (s[1] == 'u' && s + 5 < end) {
		int ch = 0;
		for (int i = 2; i < 6; ++i) {
		    char c = s[i];
		    if (c >= '0' && c <= '9')
			ch = 16 * ch + c - '0';
		    else if (c >= 'A' && c <= 'F')
			ch = 16 * ch + c - 'A' + 10;
		    else if (c >= 'a' && c <= 'f')
			ch = 16 * ch + c - 'a' + 10;
		    else
			return 0;
		}
		// special handling required for surrogate pairs
		if (unlikely(ch >= 0xD800 && ch <= 0xDFFF)) {
		    if (ch >= 0xDC00 || s + 11 >= end || s[6] != '\\' || s[7] != 'u')
			return 0;
		    int ch2 = 0;
		    for (int i = 8; i < 12; ++i) {
			char c = s[i];
			if (c >= '0' && c <= '9')
			    ch2 = 16 * ch2 + c - '0';
			else if (c >= 'A' && c <= 'F')
			    ch2 = 16 * ch2 + c - 'A' + 10;
			else if (c >= 'a' && c <= 'f')
			    ch2 = 16 * ch2 + c - 'a' + 10;
			else
			    return 0;
		    }
		    if (ch2 < 0xDC00 || ch2 > 0xDFFF)
			return 0;
		    else if (!sa.append_utf8(0x100000 + (ch - 0xD800) * 0x400 + (ch2 - 0xDC00)))
			return 0;
		    s += 11, last = s + 1;
		} else {
		    if (!sa.append_utf8(ch))
			return 0;
		    s += 5, last = s + 1;
		}
	    } else
		return 0;
	} else if (*s == '\"')
	    break;
	else if (likely((unsigned char) *s >= 32 && (unsigned char) *s < 128))
	    /* OK as is */;
	else if ((unsigned char) *s < 32)
	    return 0;
	else {
	    const char *t = String::skip_utf8_char(s, end);
	    if (t == s)
		return 0;
	    s = t - 1;
	}
    }
    if (s == end)
	return 0;
    else if (!sa.empty()) {
	sa.append(last, s);
	result = sa.take_string();
    } else if (last == s)
	result = String();
    else if (last >= str.begin() && s <= str.end())
	result = str.substring(last, s);
    else
	result = String(last, s);
    return s + 1;
}

const char *
Json::parse_primitive(const String &str, const char *begin, const char *end)
{
    if (_cjson)
	_cjson->deref(_type);
    _cjson = 0;

    const char *s = begin;
    switch (*s) {
    case '-':
	if (s + 1 == end || s[1] < '0' || s[1] > '9')
	    return 0;
	++s;
	/* fallthru */
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
	json_type type = j_int;
	if (*s == '0')
	    ++s;
	else
	    for (++s; s != end && isdigit((unsigned char) *s); )
		++s;
	if (s != end && *s == '.') {
	    type = j_double;
	    if (s + 1 == end || s[1] < '0' || s[1] > '9')
		return 0;
	    for (s += 2; s != end && isdigit((unsigned char) *s); )
		++s;
	}
	if (s != end && (*s == 'e' || *s == 'E')) {
	    type = j_double;
	    ++s;
	    if (s != end && (*s == '+' || *s == '-'))
		++s;
	    if (s == end || s[1] < '0' || s[1] > '9')
		return 0;
	    for (++s; s != end && isdigit((unsigned char) *s); )
		++s;
	}
	if (begin >= str.begin() && s <= str.end())
	    _str = str.substring(begin, s);
	else
	    _str = String(begin, s);
	_type = type;
	return s;
    }
    case 't':
	if (s + 4 <= end && s[1] == 'r' && s[2] == 'u' && s[3] == 'e') {
	    _str = String(true);
	    _type = j_bool;
	    return s + 4;
	} else
	    return 0;
    case 'f':
	if (s + 5 <= end && s[1] == 'a' && s[2] == 'l' && s[3] == 's' && s[4] == 'e') {
	    _str = String(false);
	    _type = j_bool;
	    return s + 5;
	} else
	    return 0;
    case 'n':
	if (s + 4 <= end && s[1] == 'u' && s[2] == 'l' && s[3] == 'l') {
	    _str = String();
	    _type = j_null;
	    return s + 4;
	} else
	    return 0;
    default:
	return 0;
    }
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Json)
