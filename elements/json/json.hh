// -*- c-basic-offset: 4 -*-
#ifndef CLICK_JSON_HH
#define CLICK_JSON_HH 1
#include <click/straccum.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include <click/glue.hh>
#include <click/pair.hh>
#if CLICK_USERLEVEL
# include <iterator>
#endif
CLICK_DECLS

template <typename P> class Json_proxy_base;
template <typename T> class Json_object_proxy;
template <typename T> class Json_object_str_proxy;
template <typename T> class Json_array_proxy;
class Json_get_proxy;

class Json { public:

    enum json_type { // order matters
	j_null = 0, j_array, j_object, j_int, j_double, j_bool, j_string
    };

    typedef int size_type;

    typedef Pair<const String, Json> object_value_type;
    class object_iterator;
    class const_object_iterator;

    typedef Json array_value_type;
    class array_iterator;
    class const_array_iterator;

    typedef object_iterator iterator;
    typedef const_object_iterator const_iterator;

    typedef bool (Json::*unspecified_bool_type)() const;
    struct unparse_manipulator;

    // Constructors
    inline Json();
    inline Json(const Json &x);
#if HAVE_CXX_RVALUE_REFERENCES
    inline Json(Json &&x);
#endif
    inline Json(int x);
    inline Json(unsigned x);
    inline Json(long x);
    inline Json(unsigned long x);
#if HAVE_LONG_LONG
    inline Json(long long x);
    inline Json(unsigned long long x);
#endif
#if HAVE_FLOAT_TYPES
    inline Json(double x);
#endif
    inline Json(bool x);
    inline Json(const String &x);
    inline Json(const char *x);
    template <typename T> inline Json(const Vector<T> &x);
    template <typename T> inline Json(T first, T last);
    template <typename T> inline Json(const HashTable<String, T> &x);
    inline ~Json();

    static inline const Json &make_null();
    static inline Json make_array();
    static inline Json make_object();
    static inline Json make_string(const String &x);
    static inline Json make_string(const char *s, int len);

    static void static_initialize();

    // Type information
    inline operator unspecified_bool_type() const;
    inline bool operator!() const;

    inline json_type type() const;
    inline bool is_null() const;
    inline bool is_int() const;
    inline bool is_double() const;
    inline bool is_number() const;
    inline bool is_bool() const;
    inline bool is_string() const;
    inline bool is_array() const;
    inline bool is_object() const;
    inline bool is_primitive() const;

    inline bool empty() const;
    inline size_type size() const;

    // Primitive extractors
    inline long to_i() const;
    inline uint64_t to_u64() const;
    bool to_i(int &x) const;
    bool to_i(unsigned &x) const;
    bool to_i(long &x) const;
    bool to_i(unsigned long &x) const;
#if HAVE_LONG_LONG
    bool to_i(long long &x) const;
    bool to_i(unsigned long long &x) const;
#endif
    inline long as_i() const;

#if HAVE_FLOAT_TYPES
    double to_d() const;
    inline bool to_d(double &x) const;
    inline double as_d() const;
#endif

    inline bool to_b() const;
    inline bool to_b(bool &x) const;
    inline bool as_b() const;

    inline const String &to_s() const;
    inline bool to_s(String &x) const;
    inline const String &as_s() const;
    inline const char *c_str() const;

    // Object methods
    inline size_type count(const StringRef &key) const;
    inline const Json &get(const StringRef &key) const;
    inline Json &get_insert(const String &key);
    inline Json &get_insert(const StringRef &key);
    inline Json &get_insert(const char *key);

    inline long get_i(const StringRef &key) const;
#if HAVE_FLOAT_TYPES
    inline double get_d(const StringRef &key) const;
#endif
    inline bool get_b(const StringRef &key) const;
    inline const String &get_s(const StringRef &key) const;

    inline const Json_get_proxy get(const StringRef &key, Json &x) const;
    inline const Json_get_proxy get(const StringRef &key, int &x) const;
    inline const Json_get_proxy get(const StringRef &key, unsigned &x) const;
    inline const Json_get_proxy get(const StringRef &key, long &x) const;
    inline const Json_get_proxy get(const StringRef &key, unsigned long &x) const;
#if HAVE_LONG_LONG
    inline const Json_get_proxy get(const StringRef &key, long long &x) const;
    inline const Json_get_proxy get(const StringRef &key, unsigned long long &x) const;
#endif
#if HAVE_FLOAT_TYPES
    inline const Json_get_proxy get(const StringRef &key, double &x) const;
#endif
    inline const Json_get_proxy get(const StringRef &key, bool &x) const;
    inline const Json_get_proxy get(const StringRef &key, String &x) const;

    const Json &operator[](const StringRef &key) const;
    inline Json_object_proxy<Json> operator[](const String &key);
    inline Json_object_str_proxy<Json> operator[](const StringRef &key);
    inline Json_object_str_proxy<Json> operator[](const char *key);

    inline const Json &at(const StringRef &key) const;
    inline Json &at_insert(const String &key);
    inline Json &at_insert(const StringRef &key);
    inline Json &at_insert(const char *key);

    template <typename T> inline Json &set(const String &key, T value);
#if HAVE_CXX_RVALUE_REFERENCES
    inline Json &set(const String &key, Json &&x);
#endif
    inline Json &unset(const StringRef &key);

    inline Pair<object_iterator, bool> insert(const object_value_type &x);
    inline object_iterator insert(object_iterator position,
				  const object_value_type &x);
    inline size_type erase(const StringRef &key);

    inline Json &merge(const Json &x);
    template <typename P> inline Json &merge(const Json_proxy_base<P> &x);

    // Array methods
    inline const Json &get(size_type x) const;
    inline Json &get_insert(size_type x);

    inline const Json &operator[](size_type x) const;
    inline Json_array_proxy<Json> operator[](size_type x);

    inline const Json &at(size_type x) const;
    inline Json &at_insert(size_type x);

    inline const Json &back() const;
    inline Json &back();

    template <typename T> inline Json &push_back(T x);
#if HAVE_CXX_RVALUE_REFERENCES
    inline Json &push_back(Json &&x);
#endif
    inline void pop_back();

    // Iteration
    inline const_object_iterator obegin() const;
    inline const_object_iterator oend() const;
    inline object_iterator obegin();
    inline object_iterator oend();
    inline const_object_iterator cobegin() const;
    inline const_object_iterator coend() const;

    inline const_array_iterator abegin() const;
    inline const_array_iterator aend() const;
    inline array_iterator abegin();
    inline array_iterator aend();
    inline const_array_iterator cabegin() const;
    inline const_array_iterator caend() const;

    inline const_iterator begin() const;
    inline const_iterator end() const;
    inline iterator begin();
    inline iterator end();
    inline const_iterator cbegin() const;
    inline const_iterator cend() const;

    // Unparsing
    static inline unparse_manipulator indent_depth(int x);
    static inline unparse_manipulator tab_width(int x);

    inline String unparse() const;
    inline String unparse(bool add_newline) const;
    inline String unparse(const unparse_manipulator &m,
			  bool add_newline = false) const;
    void unparse(StringAccum &sa) const;
    inline void unparse(StringAccum &sa, const unparse_manipulator &m) const;

    // Parsing
    inline bool assign_parse(const String &str);
    inline bool assign_parse(const char *first, const char *last);

    static inline Json parse(const String &str);
    static inline Json parse(const char *first, const char *last);

    // Assignment
    inline Json &operator=(const Json &x);
#if HAVE_CXX_RVALUE_REFERENCES
    inline Json &operator=(Json &&x);
#endif
    template <typename P> inline Json &operator=(const Json_proxy_base<P> &x);

    inline Json &operator++();
    inline void operator++(int);
    inline Json &operator--();
    inline void operator--(int);
    inline Json &operator+=(int x);
    inline Json &operator+=(long x);
#if HAVE_FLOAT_TYPES
    inline Json &operator+=(double x);
#endif

    inline void swap(Json &x);

  private:

    enum {
	st_initial = 0, st_array_initial = 1, st_array_delim = 2,
	st_array_value = 3, st_object_initial = 4, st_object_delim = 5,
	st_object_key = 6, st_object_colon = 7, st_object_value = 8,
	max_depth = 2048
    };

    typedef Vector<Json> JsonVector;

    struct ComplexJson {
	int refcount;
	ComplexJson()
	    : refcount(1) {
	}
	void ref() {
	    ++refcount;
	}
	inline void deref(json_type j);
      private:
	ComplexJson(const ComplexJson &x); // does not exist
    };

    struct ArrayJson;
    struct ObjectItem;
    struct ObjectJson;

    json_type _type;
    String _str;
    ComplexJson *_cjson;

    inline ObjectJson *ojson() const;
    inline ArrayJson *ajson() const;

    long hard_to_i() const;
    long hard_as_i() const;
    uint64_t hard_to_u64() const;
    bool hard_to_b() const;
    const String &hard_to_s() const;
    inline void force_number();

    void hard_uniqueify(json_type type);
    inline bool uniqueify_object();
    inline bool uniqueify_array();

    bool unparse_is_complex() const;
    static void unparse_indent(StringAccum &sa, const unparse_manipulator &m, int depth);
    void hard_unparse(StringAccum &sa, const unparse_manipulator &m, int depth) const;

    static inline const char *skip_space(const char *s, const char *end);
    bool assign_parse(const String &str, const char *begin, const char *end);
    static const char *parse_string(String &result, const String &str, const char *s, const char *end);
    const char *parse_primitive(const String &str, const char *s, const char *end);

    friend class object_iterator;
    friend class const_object_iterator;
    friend class array_iterator;
    friend class const_array_iterator;
    friend bool operator==(const Json &a, const Json &b);

    struct JsonStatics;
    static char statics[];

};


struct Json::ArrayJson : public ComplexJson {
    JsonVector values;
    ArrayJson() {
    }
    ArrayJson(const ArrayJson &x)
	: ComplexJson(), values(x.values) {
    }
};

struct Json::ObjectItem {
    Pair<const String, Json> v_;
    int next_;
    explicit ObjectItem(const String &key, const Json &value, int next)
	: v_(key, value), next_(next) {
    }
};

struct Json::ObjectJson : public ComplexJson {
    ObjectItem *os_;
    int n_;
    int capacity_;
    Vector<int> hash_;
    int nremoved_;
    ObjectJson()
	: os_(), n_(0), capacity_(0), nremoved_(0) {
    }
    ObjectJson(const ObjectJson &x);
    ~ObjectJson();
    void grow(bool copy);
    int bucket(const char *s, int len) const {
	return String::hashcode(s, s + len) & (hash_.size() - 1);
    }
    ObjectItem &item(int i) const {
	return os_[i];
    }
    int find(const char *s, int len) const {
	if (hash_.size()) {
	    int i = hash_[bucket(s, len)];
	    while (i >= 0) {
		ObjectItem &oi = item(i);
		if (oi.v_.first.equals(s, len))
		    return i;
		i = oi.next_;
	    }
	}
	return -1;
    }
    int find_insert(const String &key, const Json &value);
    inline Json &get_insert(const String &key) {
	int i = find_insert(key, make_null());
	return item(i).v_.second;
    }
    Json &get_insert(const StringRef &key);
    size_type erase(const StringRef &key);
    void rehash();
};

struct CLICK_MAY_ALIAS Json::JsonStatics {
    Json null_json;
    String array_string;
    String object_string;
    inline JsonStatics();
};

inline const Json &Json::make_null() {
    return reinterpret_cast<JsonStatics *>(statics)->null_json;
}

inline void Json::ComplexJson::deref(json_type j) {
    if (--refcount <= 0) {
	if (j == j_object)
	    delete static_cast<ObjectJson *>(this);
	else
	    delete static_cast<ArrayJson *>(this);
    }
}

inline Json::ArrayJson *Json::ajson() const {
    assert(_type == j_null || _type == j_array);
    return static_cast<ArrayJson *>(_cjson);
}

inline Json::ObjectJson *Json::ojson() const {
    assert(_type == j_null || _type == j_object);
    return static_cast<ObjectJson *>(_cjson);
}

inline bool Json::uniqueify_object() {
    bool need = !_cjson || _cjson->refcount > 1;
    if (need)
	hard_uniqueify(j_object);
    assert(_type == j_object);
    return need;
}

inline bool Json::uniqueify_array() {
    bool need = !_cjson || _cjson->refcount > 1;
    if (need)
	hard_uniqueify(j_array);
    assert(_type == j_array);
    return need;
}


class Json::const_object_iterator { public:
    typedef Pair<const String, Json> value_type;
    typedef const value_type *pointer_type;
    typedef const value_type &reference_type;
#if CLICK_USERLEVEL
    typedef std::forward_iterator_tag iterator_category;
#endif

    const_object_iterator() {
    }
    typedef bool (const_object_iterator::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const {
	return live() ? &const_object_iterator::live : 0;
    }
    bool live() const {
	return i_ >= 0;
    }
    const value_type &operator*() const {
	return j_->ojson()->item(i_).v_;
    }
    const value_type *operator->() const {
	return &(**this);
    }
    const String &key() const {
	return (**this).first;
    }
    const Json &value() const {
	return (**this).second;
    }
    void operator++() {
	++i_;
	fix();
    }
    void operator++(int) {
	++(*this);
    }
  private:
    const Json *j_;
    int i_;
    const_object_iterator(const Json *j, int i)
	: j_(j), i_(i) {
	if (i_ >= 0)
	    fix();
    }
    void fix() {
	ObjectJson *oj = j_->ojson();
    retry:
	if (!oj || i_ >= oj->n_)
	    i_ = -1;
	else if (oj->item(i_).next_ == -2) {
	    ++i_;
	    goto retry;
	}
    }
    friend class Json;
    friend bool operator==(const const_object_iterator &, const const_object_iterator &);
};

class Json::object_iterator : public const_object_iterator { public:
    typedef value_type *pointer_type;
    typedef value_type &reference_type;

    object_iterator() {
    }
    value_type &operator*() const {
	const_cast<Json *>(j_)->uniqueify_object();
	return j_->ojson()->item(i_).v_;
    }
    value_type *operator->() const {
	return &(**this);
    }
    Json &value() const {
	return (**this).second;
    }
  private:
    object_iterator(Json *j, int i)
	: const_object_iterator(j, i) {
    }
    friend class Json;
};

inline bool operator==(const Json::const_object_iterator &a, const Json::const_object_iterator &b) {
    return a.j_ == b.j_ && a.i_ == b.i_;
}

inline bool operator!=(const Json::const_object_iterator &a, const Json::const_object_iterator &b) {
    return !(a == b);
}

class Json::const_array_iterator { public:
    typedef Json::size_type difference_type;
    typedef Json value_type;
    typedef const Json *pointer_type;
    typedef const Json &reference_type;
#if CLICK_USERLEVEL
    typedef std::random_access_iterator_tag iterator_category;
#endif

    const_array_iterator() {
    }
    typedef bool (const_array_iterator::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const {
	return live() ? &const_array_iterator::live : 0;
    }
    bool live() const {
	return j_->_type == j_array && JsonVector::size_type(i_) < j_->ajson()->values.size();
    }
    const Json &operator*() const {
	return j_->ajson()->values[i_];
    }
    const Json &operator[](difference_type i) const {
	return j_->ajson()->values[i_ + i];
    }
    const Json *operator->() const {
	return &(**this);
    }
    const Json &value() const {
	return **this;
    }
    void operator++(int) {
	++i_;
    }
    void operator++() {
	++i_;
    }
    void operator--(int) {
	--i_;
    }
    void operator--() {
	--i_;
    }
    const_array_iterator &operator+=(difference_type x) {
	i_ += x;
	return *this;
    }
    const_array_iterator &operator-=(difference_type x) {
	i_ -= x;
	return *this;
    }
  private:
    const Json *j_;
    int i_;
    const_array_iterator(const Json *j, int i)
	: j_(j), i_(i) {
    }
    friend class Json;
    friend class Json::array_iterator;
    friend bool operator==(const const_array_iterator &, const const_array_iterator &);
    friend bool operator<(const const_array_iterator &, const const_array_iterator &);
    friend difference_type operator-(const const_array_iterator &, const const_array_iterator &);
};

class Json::array_iterator : public const_array_iterator { public:
    typedef const Json *pointer_type;
    typedef const Json &reference_type;

    array_iterator() {
    }
    Json &operator*() const {
	const_cast<Json *>(j_)->uniqueify_array();
	return j_->ajson()->values[i_];
    }
    Json &operator[](difference_type i) const {
	const_cast<Json *>(j_)->uniqueify_array();
	return j_->ajson()->values[i_ + i];
    }
    Json *operator->() const {
	return &(**this);
    }
    Json &value() const {
	return **this;
    }
  private:
    array_iterator(Json *j, int i)
	: const_array_iterator(j, i) {
    }
    friend class Json;
};

inline bool operator==(const Json::const_array_iterator &a, const Json::const_array_iterator &b) {
    return a.j_ == b.j_ && a.i_ == b.i_;
}

inline bool operator<(const Json::const_array_iterator &a, const Json::const_array_iterator &b) {
    return a.j_ < b.j_ || (a.j_ == b.j_ && a.i_ < b.i_);
}

inline bool operator!=(const Json::const_array_iterator &a, const Json::const_array_iterator &b) {
    return !(a == b);
}

inline bool operator<=(const Json::const_array_iterator &a, const Json::const_array_iterator &b) {
    return !(b < a);
}

inline bool operator>(const Json::const_array_iterator &a, const Json::const_array_iterator &b) {
    return b < a;
}

inline bool operator>=(const Json::const_array_iterator &a, const Json::const_array_iterator &b) {
    return !(a < b);
}

inline Json::const_array_iterator operator+(Json::const_array_iterator a, Json::const_array_iterator::difference_type i) {
    return a += i;
}

inline Json::const_array_iterator operator-(Json::const_array_iterator a, Json::const_array_iterator::difference_type i) {
    return a -= i;
}

inline Json::const_array_iterator::difference_type operator-(const Json::const_array_iterator &a, const Json::const_array_iterator &b) {
    assert(a.j_ == b.j_);
    return a.i_ - b.i_;
}


template <typename P>
class Json_proxy_base {
  public:
    const Json &cvalue() const {
	return static_cast<const P *>(this)->cvalue();
    }
    Json &value() {
	return static_cast<P *>(this)->value();
    }
    operator const Json &() const {
	return cvalue();
    }
    operator Json &() {
	return value();
    }
    operator Json::unspecified_bool_type() const {
	return cvalue();
    }
    bool operator!() const {
	return !cvalue();
    }
    Json::json_type type() const {
	return cvalue().type();
    }
    bool is_null() const {
	return cvalue().is_null();
    }
    bool is_int() const {
	return cvalue().is_int();
    }
    bool is_double() const {
	return cvalue().is_double();
    }
    bool is_number() const {
	return cvalue().is_number();
    }
    bool is_bool() const {
	return cvalue().is_bool();
    }
    bool is_string() const {
	return cvalue().is_string();
    }
    bool is_array() const {
	return cvalue().is_array();
    }
    bool is_object() const {
	return cvalue().is_object();
    }
    bool is_primitive() const {
	return cvalue().is_primitive();
    }
    bool empty() const {
	return cvalue().empty();
    }
    Json::size_type size() const {
	return cvalue().size();
    }
    long to_i() const {
	return cvalue().to_i();
    }
    uint64_t to_u64() const {
	return cvalue().to_u64();
    }
    bool to_i(int &x) const {
	return cvalue().to_i(x);
    }
    bool to_i(unsigned &x) const {
	return cvalue().to_i(x);
    }
    bool to_i(long &x) const {
	return cvalue().to_i(x);
    }
    bool to_i(unsigned long &x) const {
	return cvalue().to_i(x);
    }
#if HAVE_LONG_LONG
    bool to_i(long long &x) const {
	return cvalue().to_i(x);
    }
    bool to_i(unsigned long long &x) const {
	return cvalue().to_i(x);
    }
#endif
    long as_i() const {
	return cvalue().as_i();
    }
#if HAVE_FLOAT_TYPES
    double to_d() const {
	return cvalue().to_d();
    }
    bool to_d(double &x) const {
	return cvalue().to_d(x);
    }
    double as_d() const {
	return cvalue().as_d();
    }
#endif
    bool to_b() const {
	return cvalue().to_b();
    }
    bool to_b(bool &x) const {
	return cvalue().to_b(x);
    }
    bool as_b() const {
	return cvalue().as_b();
    }
    const String &to_s() const {
	return cvalue().to_s();
    }
    bool to_s(String &x) const {
	return cvalue().to_s(x);
    }
    const String &as_s() const {
	return cvalue().as_s();
    }
    const char *c_str() const {
	return cvalue().c_str();
    }
    Json::size_type count(const StringRef &key) const {
	return cvalue().count(key);
    }
    const Json &get(const StringRef &key) const {
	return cvalue().get(key);
    }
    Json &get_insert(const String &key) {
	return value().get_insert(key);
    }
    Json &get_insert(const StringRef &key) {
	return value().get_insert(key);
    }
    Json &get_insert(const char *key) {
	return value().get_insert(key);
    }
    long get_i(const StringRef &key) const {
	return cvalue().get_i(key);
    }
#if HAVE_FLOAT_TYPES
    double get_d(const StringRef &key) const {
	return cvalue().get_d(key);
    }
#endif
    bool get_b(const StringRef &key) const {
	return cvalue().get_b(key);
    }
    const String &get_s(const StringRef &key) const {
	return cvalue().get_s(key);
    }
    inline const Json_get_proxy get(const StringRef &key, Json &x) const;
    inline const Json_get_proxy get(const StringRef &key, int &x) const;
    inline const Json_get_proxy get(const StringRef &key, unsigned &x) const;
    inline const Json_get_proxy get(const StringRef &key, long &x) const;
    inline const Json_get_proxy get(const StringRef &key, unsigned long &x) const;
#if HAVE_LONG_LONG
    inline const Json_get_proxy get(const StringRef &key, long long &x) const;
    inline const Json_get_proxy get(const StringRef &key, unsigned long long &x) const;
#endif
#if HAVE_FLOAT_TYPES
    inline const Json_get_proxy get(const StringRef &key, double &x) const;
#endif
    inline const Json_get_proxy get(const StringRef &key, bool &x) const;
    inline const Json_get_proxy get(const StringRef &key, String &x) const;
    const Json &operator[](const StringRef &key) const {
	return cvalue().get(key);
    }
    Json_object_proxy<P> operator[](const String &key) {
	return Json_object_proxy<P>(*static_cast<P *>(this), key);
    }
    Json_object_str_proxy<P> operator[](const StringRef &key) {
	return Json_object_str_proxy<P>(*static_cast<P *>(this), key);
    }
    Json_object_str_proxy<P> operator[](const char *key) {
	return Json_object_str_proxy<P>(*static_cast<P *>(this), key);
    }
    const Json &at(const StringRef &key) const {
	return cvalue().at(key);
    }
    Json &at_insert(const String &key) {
	return value().at_insert(key);
    }
    Json &at_insert(const StringRef &key) {
	return value().at_insert(key);
    }
    Json &at_insert(const char *key) {
	return value().at_insert(key);
    }
    template <typename T> inline Json &set(const String &key, T value) {
	return this->value().set(key, value);
    }
#if HAVE_CXX_RVALUE_REFERENCES
    inline Json &set(const String &key, Json &&value) {
	return this->value().set(key, click_move(value));
    }
#endif
    Json &unset(const StringRef &key) {
	return value().unset(key);
    }
    Pair<Json::object_iterator, bool> insert(const Json::object_value_type &x) {
	return value().insert(x);
    }
    Json::object_iterator insert(Json::object_iterator position, const Json::object_value_type &x) {
	return value().insert(position, x);
    }
    Json::size_type erase(const StringRef &key) {
	return value().erase(key);
    }
    Json &merge(const Json &x) {
	return value().merge(x);
    }
    template <typename P2> Json &merge(const Json_proxy_base<P2> &x) {
	return value().merge(x.cvalue());
    }
    const Json &get(Json::size_type x) const {
	return cvalue().get(x);
    }
    Json &get_insert(Json::size_type x) {
	return value().get_insert(x);
    }
    const Json &operator[](int key) const {
	return cvalue().at(key);
    }
    Json_array_proxy<P> operator[](int key) {
	return Json_array_proxy<P>(*static_cast<P *>(this), key);
    }
    const Json &at(Json::size_type x) const {
	return cvalue().at(x);
    }
    Json &at_insert(Json::size_type x) {
	return value().at_insert(x);
    }
    const Json &back() const {
	return cvalue().back();
    }
    Json &back() {
	return value().back();
    }
    template <typename T> Json &push_back(T x) {
	return value().push_back(x);
    }
#if HAVE_CXX_RVALUE_REFERENCES
    Json &push_back(Json &&x) {
	return value().push_back(click_move(x));
    }
#endif
    void pop_back() {
	value().pop_back();
    }
    void unparse(StringAccum &sa) const {
	return cvalue().unparse(sa);
    }
    void unparse(StringAccum &sa, const Json::unparse_manipulator &m) const {
	return cvalue().unparse(sa, m);
    }
    String unparse() const {
	return cvalue().unparse(false);
    }
    String unparse(bool add_newline) const {
	return cvalue().unparse(add_newline);
    }
    String unparse(const Json::unparse_manipulator &m, bool add_newline = false) const {
	return cvalue().unparse(m, add_newline);
    }
    bool assign_parse(const String &str) {
	return value().assign_parse(str);
    }
    bool assign_parse(const char *first, const char *last) {
	return value().assign_parse(first, last);
    }
    Json &operator++() {
	return ++value();
    }
    void operator++(int) {
	value()++;
    }
    Json &operator--() {
	return --value();
    }
    void operator--(int) {
	value()--;
    }
    Json &operator+=(int x) {
	return value() += x;
    }
    Json &operator+=(long x) {
	return value() += x;
    }
#if HAVE_FLOAT_TYPES
    Json &operator+=(double x) {
	return value() += x;
    }
#endif
    Json::const_object_iterator obegin() const {
	return cvalue().obegin();
    }
    Json::const_object_iterator oend() const {
	return cvalue().oend();
    }
    Json::object_iterator obegin() {
	return value().obegin();
    }
    Json::object_iterator oend() {
	return value().oend();
    }
    Json::const_object_iterator cobegin() const {
	return cvalue().cobegin();
    }
    Json::const_object_iterator coend() const {
	return cvalue().coend();
    }
    Json::const_array_iterator abegin() const {
	return cvalue().abegin();
    }
    Json::const_array_iterator aend() const {
	return cvalue().aend();
    }
    Json::array_iterator abegin() {
	return value().abegin();
    }
    Json::array_iterator aend() {
	return value().aend();
    }
    Json::const_array_iterator cabegin() const {
	return cvalue().cabegin();
    }
    Json::const_array_iterator caend() const {
	return cvalue().caend();
    }
    Json::const_iterator begin() const {
	return cvalue().begin();
    }
    Json::const_iterator end() const {
	return cvalue().end();
    }
    Json::iterator begin() {
	return value().begin();
    }
    Json::iterator end() {
	return value().end();
    }
    Json::const_iterator cbegin() const {
	return cvalue().cbegin();
    }
    Json::const_iterator cend() const {
	return cvalue().cend();
    }
};

template <typename T>
class Json_object_proxy : public Json_proxy_base<Json_object_proxy<T> > {
  public:
    const Json &cvalue() const {
	return base_.get(key_);
    }
    Json &value() {
	return base_.get_insert(key_);
    }
    Json &operator=(const Json &x) {
	return value() = x;
    }
#if HAVE_CXX_RVALUE_REFERENCES
    Json &operator=(Json &&x) {
	return value() = click_move(x);
    }
#endif
    Json &operator=(const Json_object_proxy<T> &x) {
	return value() = x.cvalue();
    }
    template <typename P> Json &operator=(const Json_proxy_base<P> &x) {
	return value() = x.cvalue();
    }
    Json_object_proxy(T &ref, const String &key)
	: base_(ref), key_(key) {
    }
    T &base_;
    String key_;
};

template <typename T>
class Json_object_str_proxy : public Json_proxy_base<Json_object_str_proxy<T> > {
  public:
    const Json &cvalue() const {
	return base_.get(key_);
    }
    Json &value() {
	return base_.get_insert(key_);
    }
    Json &operator=(const Json &x) {
	return value() = x;
    }
#if HAVE_CXX_RVALUE_REFERENCES
    Json &operator=(Json &&x) {
	return value() = click_move(x);
    }
#endif
    Json &operator=(const Json_object_str_proxy<T> &x) {
	return value() = x.cvalue();
    }
    template <typename P> Json &operator=(const Json_proxy_base<P> &x) {
	return value() = x.cvalue();
    }
    Json_object_str_proxy(T &ref, const StringRef &key)
	: base_(ref), key_(key) {
    }
    T &base_;
    StringRef key_;
};

template <typename T>
class Json_array_proxy : public Json_proxy_base<Json_array_proxy<T> > {
  public:
    const Json &cvalue() const {
	return base_.get(key_);
    }
    Json &value() {
	return base_.get_insert(key_);
    }
    Json &operator=(const Json &x) {
	return value() = x;
    }
#if HAVE_CXX_RVALUE_REFERENCES
    Json &operator=(Json &&x) {
	return value() = click_move(x);
    }
#endif
    Json &operator=(const Json_array_proxy<T> &x) {
	return value() = x.cvalue();
    }
    template <typename P> Json &operator=(const Json_proxy_base<P> &x) {
	return value() = x.cvalue();
    }
    Json_array_proxy(T &ref, int key)
	: base_(ref), key_(key) {
    }
    T &base_;
    int key_;
};

class Json_get_proxy : public Json_proxy_base<Json_get_proxy> {
  public:
    const Json &cvalue() const {
	return base_;
    }
    operator Json::unspecified_bool_type() const {
	return status_ ? &Json::is_null : 0;
    }
    bool operator!() const {
	return !status_;
    }
    bool status() const {
	return status_;
    }
    const Json_get_proxy &status(bool &x) const {
	x = status_;
	return *this;
    }
    Json_get_proxy(const Json &ref, bool status)
	: base_(ref), status_(status) {
    }
    const Json &base_;
    bool status_;
  private:
    Json_get_proxy &operator=(const Json_get_proxy &x);
};

template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, Json &x) const {
    return cvalue().get(key, x);
}
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, int &x) const {
    return cvalue().get(key, x);
}
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, unsigned &x) const {
    return cvalue().get(key, x);
}
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, long &x) const {
    return cvalue().get(key, x);
}
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, unsigned long &x) const {
    return cvalue().get(key, x);
}
#if HAVE_LONG_LONG
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, long long &x) const {
    return cvalue().get(key, x);
}
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, unsigned long long &x) const {
    return cvalue().get(key, x);
}
#endif
#if HAVE_FLOAT_TYPES
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, double &x) const {
    return cvalue().get(key, x);
}
#endif
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, bool &x) const {
    return cvalue().get(key, x);
}
template <typename T>
inline const Json_get_proxy Json_proxy_base<T>::get(const StringRef &key, String &x) const {
    return cvalue().get(key, x);
}


/** @brief Construct a null Json. */
inline Json::Json()
    : _type(j_null), _cjson() {
}
/** @brief Construct a Json copy of @a x. */
inline Json::Json(const Json &x)
    : _type(x._type), _str(x._str), _cjson(x._cjson) {
    if (_cjson)
	_cjson->ref();
}
#if HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline Json::Json(Json &&x)
    : _type(x._type), _str(click_move(x._str)), _cjson(x._cjson) {
    x._cjson = 0;
}
#endif
/** @brief Construct simple Json values. */
inline Json::Json(int x)
    : _type(j_int), _str(x), _cjson() {
}
inline Json::Json(unsigned x)
    : _type(j_int), _str(x), _cjson() {
}
inline Json::Json(long x)
    : _type(j_int), _str(x), _cjson() {
}
inline Json::Json(unsigned long x)
    : _type(j_int), _str(x), _cjson() {
}
#if HAVE_LONG_LONG
inline Json::Json(long long x)
    : _type(j_int), _str(x), _cjson() {
}
inline Json::Json(unsigned long long x)
    : _type(j_int), _str(x), _cjson() {
}
#endif
#if HAVE_FLOAT_TYPES
inline Json::Json(double x)
    : _type(j_double), _str(x), _cjson() {
}
#endif
inline Json::Json(bool x)
    : _type(j_bool), _str(x), _cjson() {
}
inline Json::Json(const String &x)
    : _type(j_string), _str(x), _cjson() {
}
inline Json::Json(const char *x)
    : _type(j_string), _str(x), _cjson() {
}
/** @brief Construct an array Json containing the elements of @a x. */
template <typename T>
inline Json::Json(const Vector<T> &x)
    : _type(j_array), _cjson(new ArrayJson) {
    for (const T *it = x.begin(); it != x.end(); ++it)
	ajson()->values.push_back(Json(*it));
}
/** @brief Construct an array Json containing the elements in [@a first,
    @a last). */
template <typename T>
inline Json::Json(T first, T last)
    : _type(j_array), _cjson(new ArrayJson) {
    while (first != last) {
	ajson()->values.push_back(Json(*first));
	++first;
    }
}
/** @brief Construct an object Json containing the values in @a x. */
template <typename T>
inline Json::Json(const HashTable<String, T> &x)
    : _type(j_object), _cjson(new ObjectJson) {
    for (typename HashTable<String, T>::const_iterator it = x.begin();
	 it != x.end(); ++it) {
	Json &x = ojson()->get_insert(it.key());
	x = Json(it.value());
    }
}
inline Json::~Json() {
    if (_cjson)
	_cjson->deref(_type);
}

/** @brief Return an empty array-valued Json. */
inline Json Json::make_array() {
    Json j;
    j._type = j_array;
    return j;
}
/** @brief Return an empty object-valued Json. */
inline Json Json::make_object() {
    Json j;
    j._type = j_object;
    return j;
}
/** @brief Return a string-valued Json. */
inline Json Json::make_string(const String &x) {
    return Json(x);
}
/** @overload */
inline Json Json::make_string(const char *s, int len) {
    return Json(String(s, len));
}

/** @brief Return true if this Json is not null.
    @sa empty() */
inline Json::operator unspecified_bool_type() const {
    return _type == j_null ? 0 : &Json::is_null;
}
/** @brief Return true if this Json is null. */
inline bool Json::operator!() const {
    return _type == j_null;
}

/** @brief Return this Json's type. */
inline Json::json_type Json::type() const {
    return _type;
}
/** @brief Test this Json's type. */
inline bool Json::is_null() const {
    return _type == j_null;
}
inline bool Json::is_int() const {
    return _type == j_int;
}
inline bool Json::is_double() const {
    return _type == j_double;
}
inline bool Json::is_number() const {
    return is_int() || is_double();
}
inline bool Json::is_bool() const {
    return _type == j_bool;
}
inline bool Json::is_string() const {
    return _type == j_string;
}
inline bool Json::is_array() const {
    return _type == j_array;
}
inline bool Json::is_object() const {
    return _type == j_object;
}
/** @brief Test if this Json is a primitive value, not including null. */
inline bool Json::is_primitive() const {
    return _type >= j_int && _type <= j_string;
}

/** @brief Return true if this Json is null, an empty array, or an empty
    object. */
inline bool Json::empty() const {
    return (_type <= j_object && size() == 0);
}
/** @brief Return the number of elements in this complex Json.
    @pre is_array() || is_object() || is_null() */
inline Json::size_type Json::size() const {
    assert(_type == j_null || _type == j_array || _type == j_object);
    if (!_cjson)
	return 0;
    else if (_type == j_object) {
	ObjectJson *oj = ojson();
	return oj->n_ - oj->nremoved_;
    } else
	return ajson()->values.size();
}


// Primitive methods

/** @brief Return this Json converted to an integer.

    Converts any Json to an integer value. Numeric Jsons convert as you'd
    expect. Null Jsons convert to 0; false boolean Jsons to 0 and true
    boolean Jsons to 1; string Jsons to a number parsed from their initial
    portions; and array and object Jsons to size().
    @sa as_i() */
inline long Json::to_i() const {
    if (_type == j_int && _str.length() == 1)
	return _str[0] - '0';
    else
	return hard_to_i();
}

/** @brief Return this Json converted to a 64-bit unsigned integer.

    See to_i() for the conversion rules. */
inline uint64_t Json::to_u64() const {
    if (_type == j_int && _str.length() == 1)
	return _str[0] - '0';
    else
	return hard_to_u64();
}

/** @brief Return the integer value of this numeric Json.
    @pre is_number()
    @sa to_i() */
inline long Json::as_i() const {
    assert(_type == j_int || _type == j_double);
    if (_str.length() == 1)
	return _str[0] - '0';
    else
	return hard_as_i();
}

#if HAVE_FLOAT_TYPES
/** @brief Extract this numeric Json's value into @a x.
    @param[out] x value storage
    @return True iff is_number().

    If !is_number(), @a x remains unchanged. */
inline bool Json::to_d(double &x) const {
    if (_type == j_double || _type == j_int) {
	x = to_d();
	return true;
    } else
	return false;
}

/** @brief Return the double value of this numeric Json.
    @pre is_number()
    @sa to_d() */
inline double Json::as_d() const {
    assert(_type == j_double || _type == j_int);
    return to_d();
}
#endif

/** @brief Return this Json converted to a boolean.

    Converts any Json to a boolean value. Boolean Jsons convert as you'd
    expect. Null Jsons convert to false; zero-valued numeric Jsons to false,
    and other numeric Jsons to true; empty string Jsons to false, and other
    string Jsons to true; and array and object Jsons to !empty().
    @sa as_b() */
inline bool Json::to_b() const {
    if (_type == j_bool)
	return _str[0] == 't';
    else
	return hard_to_b();
}

/** @brief Extract this boolean Json's value into @a x.
    @param[out] x value storage
    @return True iff is_bool().

    If !is_bool(), @a x remains unchanged. */
inline bool Json::to_b(bool &x) const {
    if (_type == j_bool) {
	x = _str[0] == 't';
	return true;
    } else
	return false;
}

/** @brief Return the value of this boolean Json.
    @pre is_bool()
    @sa to_b() */
inline bool Json::as_b() const {
    assert(_type == j_bool);
    return _str[0] == 't';
}

/** @brief Return this Json converted to a string.

    Converts any Json to a string value. String Jsons convert as you'd expect.
    Null Jsons convert to the empty string; numeric Jsons to their string
    values; boolean Jsons to "false" or "true"; and array and object Jsons to
    "[Array]" and "[Object]", respectively.
    @sa as_s() */
inline const String &Json::to_s() const {
    if (_type == j_string)
	return _str;
    else
	return hard_to_s();
}

/** @brief Extract this string Json's value into @a x.
    @param[out] x value storage
    @return True iff is_string().

    If !is_string(), @a x remains unchanged. */
inline bool Json::to_s(String &x) const {
    if (_type == j_string) {
	x = _str;
	return true;
    } else
	return false;
}

/** @brief Return the value of this string Json.
    @pre is_string()
    @sa to_s() */
inline const String &Json::as_s() const {
    assert(_type == j_string);
    return _str;
}

/** @brief Return to_s().c_str(). */
inline const char *Json::c_str() const {
    return to_s().c_str();
}

inline void Json::force_number() {
    assert(_type == j_null || _type == j_int || _type == j_double);
    if (_type == j_null) {
	_type = j_int;
	_str = String(0);
    }
}


// Object methods

/** @brief Return 1 if this object Json contains @a key, 0 otherwise.

    Returns 0 if this is not an object Json. */
inline Json::size_type Json::count(const StringRef &key) const {
    assert(_type == j_null || _type == j_object);
    return _cjson ? ojson()->find(key.data(), key.length()) >= 0 : 0;
}

/** @brief Return the value at @a key in an object Json.

    Returns a null Json if !count(@a key). */
inline const Json &Json::get(const StringRef &key) const {
    int i;
    ObjectJson *oj;
    if (_type == j_object && (oj = ojson())
	&& (i = oj->find(key.data(), key.length())) >= 0)
	return oj->item(i).v_.second;
    else
	return make_null();
}

/** @brief Return a reference to the value of @a key in an object Json.
    @pre is_object() || is_null()

    If !count(@a key), then a null Json is inserted at @a key. If is_null(),
    this Json is silently promoted to an empty object. */
inline Json &Json::get_insert(const String &key) {
    uniqueify_object();
    return ojson()->get_insert(key);
}

/** @overload */
inline Json &Json::get_insert(const StringRef &key) {
    uniqueify_object();
    return ojson()->get_insert(key);
}

/** @overload */
inline Json &Json::get_insert(const char *key) {
    uniqueify_object();
    return ojson()->get_insert(StringRef(key));
}

/** @brief Return get(@a key).to_i(). */
inline long Json::get_i(const StringRef &key) const {
    return get(key).to_i();
}

#if HAVE_FLOAT_TYPES
/** @brief Return get(@a key).to_d(). */
inline double Json::get_d(const StringRef &key) const {
    return get(key).to_d();
}
#endif

/** @brief Return get(@a key).to_b(). */
inline bool Json::get_b(const StringRef &key) const {
    return get(key).to_b();
}

/** @brief Return get(@a key).to_s(). */
inline const String &Json::get_s(const StringRef &key) const {
    return get(key).to_s();
}

/** @brief Extract this object Json's value at @a key into @a x.
    @param[out] x value storage
    @return proxy for *this

    @a x is assigned iff contains(@a key). The return value is a proxy
    object that mostly behaves like *this. However, the proxy is "truthy"
    iff contains(@a key) and @a x was assigned. The proxy also has status()
    methods that return the extraction status. For example:

    <code>
    Json j = Json::parse("{\"a\":1,\"b\":2}"), x;
    assert(j.get("a", x));            // extraction succeeded
    assert(x == Json(1));
    assert(!j.get("c", x));           // no "c" key
    assert(x == Json(1));             // x remains unchanged
    assert(!j.get("c", x).status());  // can use ".status()" for clarity

    // Can chain .get() methods to extract multiple values
    Json a, b, c;
    j.get("a", a).get("b", b);
    assert(a == Json(1) && b == Json(2));

    // Use .status() to return or assign extraction status
    bool a_status, b_status, c_status;
    j.get("a", a).status(a_status)
     .get("b", b).status(b_status)
     .get("c", c).status(c_status);
    assert(a_status && b_status && !c_status);
    </code>

    Overloaded versions of @a get() can extract integer, double, boolean,
    and string values for specific keys. These versions succeed iff
    contains(@a key) and the corresponding value has the expected type. For
    example:

    <code>
    Json j = Json::parse("{\"a\":1,\"b\":\"\"}");
    int a, b;
    bool a_status, b_status;
    j.get("a", a).status(a_status).get("b", b).status(b_status);
    assert(a_status && a == 1 && !b_status);
    </code> */
inline const Json_get_proxy Json::get(const StringRef &key, Json &x) const {
    int i;
    ObjectJson *oj;
    if (_type == j_object && (oj = ojson())
	&& (i = oj->find(key.data(), key.length())) >= 0) {
	x = oj->item(i).v_.second;
	return Json_get_proxy(*this, true);
    } else
	return Json_get_proxy(*this, false);
}

/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, int &x) const {
    return Json_get_proxy(*this, get(key).to_i(x));
}

/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, unsigned &x) const {
    return Json_get_proxy(*this, get(key).to_i(x));
}

/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, long &x) const {
    return Json_get_proxy(*this, get(key).to_i(x));
}

/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, unsigned long &x) const {
    return Json_get_proxy(*this, get(key).to_i(x));
}

#if HAVE_LONG_LONG
/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, long long &x) const {
    return Json_get_proxy(*this, get(key).to_i(x));
}

/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, unsigned long long &x) const {
    return Json_get_proxy(*this, get(key).to_i(x));
}
#endif

#if HAVE_FLOAT_TYPES
/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, double &x) const {
    return Json_get_proxy(*this, get(key).to_d(x));
}
#endif

/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, bool &x) const {
    return Json_get_proxy(*this, get(key).to_b(x));
}

/** @overload */
inline const Json_get_proxy Json::get(const StringRef &key, String &x) const {
    return Json_get_proxy(*this, get(key).to_s(x));
}


/** @brief Return the value at @a key in an object Json.

    Returns a null Json if !count(@a key). */
inline const Json &Json::operator[](const StringRef &key) const {
    return get(key);
}

/** @brief Return a proxy reference to the value at @a key in an object Json.
    @pre is_object() || is_null()

    Returns the current @a key value if it exists. Otherwise, returns a proxy
    that acts like a null Json. If this proxy is assigned, the object is
    extended as necessary to contain the new value. */
inline Json_object_proxy<Json> Json::operator[](const String &key) {
    return Json_object_proxy<Json>(*this, key);
}

/** @overload */
inline Json_object_str_proxy<Json> Json::operator[](const StringRef &key) {
    return Json_object_str_proxy<Json>(*this, key);
}

/** @overload */
inline Json_object_str_proxy<Json> Json::operator[](const char *key) {
    return Json_object_str_proxy<Json>(*this, key);
}

/** @brief Return the value at @a key in an object Json.
    @pre is_object() && count(@a key) */
inline const Json &Json::at(const StringRef &key) const {
    assert(_type == j_object && _cjson);
    ObjectJson *oj = ojson();
    int i = oj->find(key.data(), key.length());
    assert(i >= 0);
    return oj->item(i).v_.second;
}

/** @brief Return a reference to the value at @a key in an object Json.
    @pre is_object()

    Returns a newly-inserted null Json if !count(@a key). */
inline Json &Json::at_insert(const String &key) {
    assert(_type == j_object);
    return get_insert(key);
}

/** @overload */
inline Json &Json::at_insert(const StringRef &key) {
    assert(_type == j_object);
    return get_insert(key);
}

/** @overload */
inline Json &Json::at_insert(const char *key) {
    assert(_type == j_object);
    return get_insert(StringRef(key));
}

/** @brief Set the value of @a key to @a value in this object Json.
    @pre is_object() || is_null()
    @return this Json

    A null Json is promoted to an empty object. */
template <typename T> inline Json &Json::set(const String &key, T value) {
    uniqueify_object();
    ojson()->get_insert(key) = Json(value);
    return *this;
}

#if HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline Json &Json::set(const String &key, Json &&value) {
    uniqueify_object();
    ojson()->get_insert(key) = click_move(value);
    return *this;
}
#endif

/** @brief Remove the value of @a key from an object Json.
    @return this Json
    @sa erase() */
inline Json &Json::unset(const StringRef &key) {
    if (_type == j_object) {
	uniqueify_object();
	ojson()->erase(key);
    }
    return *this;
}

/** @brief Insert element @a x in this object Json.
    @param x Pair of key and value.
    @return Pair of iterator pointing to key's value and bool indicating
    whether the value is newly inserted.
    @pre is_object()

    An existing element with key @a x.first is not replaced. */
inline Pair<Json::object_iterator, bool> Json::insert(const object_value_type &x) {
    assert(_type == j_object);
    uniqueify_object();
    ObjectJson *oj = ojson();
    int n = oj->n_, i = oj->find_insert(x.first, x.second);
    return make_pair(object_iterator(this, i), i == n);
}

/** @brief Insert element @a x in this object Json.
    @param position Ignored.
    @param x Pair of key and value.
    @return Pair of iterator pointing to key's value and bool indicating
    whether the value is newly inserted.
    @pre is_object()

    An existing element with key @a x.first is not replaced. */
inline Json::object_iterator Json::insert(object_iterator position, const object_value_type &x) {
    (void) position;
    return insert(x).first;
}

/** @brief Remove the value of @a key from an object Json.
    @pre is_object()
    @return Number of items removed */
inline Json::size_type Json::erase(const StringRef &key) {
    assert(_type == j_object);
    uniqueify_object();
    return ojson()->erase(key);
}

/** @brief Merge the values of object Json @a x into this object Json.
    @pre (is_object() || is_null()) && (x.is_object() || x.is_null())
    @return this Json

    The key-value pairs in @a x are assigned to this Json. Null Jsons are
    silently converted to empty objects, except that if @a x and this Json are
    both null, then this Json is left as null. */
inline Json &Json::merge(const Json &x) {
    assert(_type == j_object || _type == j_null);
    assert(x._type == j_object || x._type == j_null);
    if (x._cjson) {
	uniqueify_object();
	ObjectJson *oj = ojson(), *xoj = x.ojson();
	const ObjectItem *xb = xoj->os_, *xe = xb + xoj->n_;
	for (; xb != xe; ++xb)
	    if (xb->next_ > -2)
		oj->get_insert(xb->v_.first) = xb->v_.second;
    }
    return *this;
}

/** @cond never */
template <typename U>
inline Json &Json::merge(const Json_proxy_base<U> &x) {
    return merge(x.cvalue());
}
/** @endcond never */


// ARRAY METHODS

/** @brief Return the @a x th array element.

    Returns a null Json if !is_array() || x >= size(). */
inline const Json &Json::get(size_type x) const {
    ArrayJson *aj;
    if (_type == j_array && (aj = ajson())
	&& JsonVector::size_type(x) < aj->values.size())
	return aj->values[x];
    else
	return make_null();
}

/** @brief Return a reference to the @a x th array element.
    @pre is_array() || is_null()

    A null Json is promoted to an array. The array is extended if @a x is out
    of range. */
inline Json &Json::get_insert(size_type x) {
    uniqueify_array();
    ArrayJson *aj = ajson();
    if (JsonVector::size_type(x) >= aj->values.size())
	aj->values.resize(x + 1);
    return aj->values[x];
}

/** @brief Return the @a x th element in an array Json.
    @pre is_array()

    A null Json is treated like an empty array. */
inline const Json &Json::at(size_type x) const {
    assert(_type == j_array);
    return get(x);
}

/** @brief Return a reference to the @a x th element in an array Json.
    @pre is_array()

    The array is extended if @a x is out of range. */
inline Json &Json::at_insert(size_type x) {
    assert(_type == j_array);
    return get_insert(x);
}

/** @brief Return the @a x th array element.

    Returns a null Json if !is_array() || x >= size(). */
inline const Json &Json::operator[](size_type x) const {
    return get(x);
}

/** @brief Return a proxy reference to the @a x th array element.
    @pre is_array() || is_null()

    Returns the current @a x th element if it exists. Otherwise, returns a
    proxy that acts like a null Json. If this proxy is assigned, the array is
    extended as necessary to contain the new value. */
inline Json_array_proxy<Json> Json::operator[](size_type x) {
    return Json_array_proxy<Json>(*this, x);
}

/** @brief Return the last array element.
    @pre is_array() && !empty() */
inline const Json &Json::back() const {
    assert(_type == j_array && _cjson && ajson()->values.size() > 0);
    return ajson()->values.back();
}

/** @brief Return a reference to the last array element.
    @pre is_array() && !empty() */
inline Json &Json::back() {
    assert(_type == j_array && _cjson && ajson()->values.size() > 0);
    uniqueify_array();
    return ajson()->values.back();
}

/** @brief Push an element onto the back of the array.
    @pre is_array() || is_null()
    @return this Json

    A null Json is promoted to an array. */
template <typename T> inline Json &Json::push_back(T x) {
    uniqueify_array();
    ajson()->values.push_back(Json(x));
    return *this;
}

#if HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline Json &Json::push_back(Json &&x) {
    uniqueify_array();
    ajson()->values.push_back(click_move(x));
    return *this;
}
#endif

/** @brief Remove the last element from an array.
    @pre is_array() && !empty() */
void Json::pop_back() {
    assert(_type == j_array && _cjson && ajson()->values.size() > 0);
    uniqueify_array();
    ajson()->values.pop_back();
}


inline Json::const_object_iterator Json::cobegin() const {
    assert(_type == j_null || _type == j_object);
    return const_object_iterator(this, 0);
}

inline Json::const_object_iterator Json::coend() const {
    assert(_type == j_null || _type == j_object);
    return const_object_iterator(this, -1);
}

inline Json::const_object_iterator Json::obegin() const {
    return cobegin();
}

inline Json::const_object_iterator Json::oend() const {
    return coend();
}

inline Json::object_iterator Json::obegin() {
    assert(_type == j_null || _type == j_object);
    return object_iterator(this, 0);
}

inline Json::object_iterator Json::oend() {
    assert(_type == j_null || _type == j_object);
    return object_iterator(this, -1);
}

inline Json::const_array_iterator Json::cabegin() const {
    assert(_type == j_null || _type == j_array);
    return const_array_iterator(this, 0);
}

inline Json::const_array_iterator Json::caend() const {
    assert(_type == j_null || _type == j_array);
    ArrayJson *aj = ajson();
    return const_array_iterator(this, aj ? aj->values.size() : 0);
}

inline Json::const_array_iterator Json::abegin() const {
    return cabegin();
}

inline Json::const_array_iterator Json::aend() const {
    return caend();
}

inline Json::array_iterator Json::abegin() {
    assert(_type == j_null || _type == j_array);
    return array_iterator(this, 0);
}

inline Json::array_iterator Json::aend() {
    assert(_type == j_null || _type == j_array);
    ArrayJson *aj = ajson();
    return array_iterator(this, aj ? aj->values.size() : 0);
}


inline Json::const_iterator Json::cbegin() const {
    return cobegin();
}

inline Json::const_iterator Json::cend() const {
    return coend();
}

inline Json::iterator Json::begin() {
    return obegin();
}

inline Json::iterator Json::end() {
    return oend();
}

inline Json::const_iterator Json::begin() const {
    return cbegin();
}

inline Json::const_iterator Json::end() const {
    return cend();
}


// Unparsing
struct Json::unparse_manipulator {
    unparse_manipulator()
	: _indent_depth(0), _tab_width(8) {
    }
    int indent_depth() const {
	return _indent_depth;
    }
    unparse_manipulator indent_depth(int x) const {
	unparse_manipulator m(*this);
	m._indent_depth = x;
	return m;
    }
    int tab_width() const {
	return _tab_width;
    }
    unparse_manipulator tab_width(int x) const {
	unparse_manipulator m(*this);
	m._tab_width = x;
	return m;
    }
  private:
    int _indent_depth;
    int _tab_width;
};

inline Json::unparse_manipulator Json::indent_depth(int x) {
    return unparse_manipulator().indent_depth(x);
}
inline Json::unparse_manipulator Json::tab_width(int x) {
    return unparse_manipulator().tab_width(x);
}

/** @brief Return the string representation of this Json. */
inline String Json::unparse() const {
    return unparse(false);
}

/** @brief Return the string representation of this Json.
    @param add_newline If true, add a final newline. */
inline String Json::unparse(bool add_newline) const {
    StringAccum sa;
    unparse(sa);
    if (add_newline)
	sa << '\n';
    return sa.take_string();
}

/** @brief Unparse the string representation of this Json into @a sa. */
inline void Json::unparse(StringAccum &sa, const unparse_manipulator &m) const {
    hard_unparse(sa, m, 0);
}

/** @brief Return the string representation of this Json.
    @param add_newline If true, add a final newline. */
inline String Json::unparse(const unparse_manipulator &m,
			    bool add_newline) const {
    StringAccum sa;
    hard_unparse(sa, m, 0);
    if (add_newline)
	sa << '\n';
    return sa.take_string();
}


// Parsing

/** @brief Parse @a str as UTF-8 JSON into this Json object.
    @return true iff the parse succeeded.

    An unsuccessful parse does not modify *this. */
inline bool Json::assign_parse(const String &str) {
    return assign_parse(str, str.begin(), str.end());
}

/** @brief Parse [@a first, @a last) as UTF-8 JSON into this Json object.
    @return true iff the parse succeeded.

    An unsuccessful parse does not modify *this. */
inline bool Json::assign_parse(const char *first, const char *last) {
    return assign_parse(String(), first, last);
}

/** @brief Return @a str parsed as UTF-8 JSON.

    Returns a null JSON object if the parse fails. */
inline Json Json::parse(const String &str) {
    Json j;
    (void) j.assign_parse(str);
    return j;
}

/** @brief Return [@a first, @a last) parsed as UTF-8 JSON.

    Returns a null JSON object if the parse fails. */
inline Json Json::parse(const char *first, const char *last) {
    Json j;
    (void) j.assign_parse(first, last);
    return j;
}


// Assignment

inline Json &Json::operator=(const Json &x) {
    _str = x._str;
    if (_cjson != x._cjson) {
	if (_cjson)
	    _cjson->deref(_type);
	_cjson = x._cjson;
	if (_cjson)
	    _cjson->ref();
    }
    _type = x._type;
    return *this;
}

#if HAVE_CXX_RVALUE_REFERENCES
inline Json &Json::operator=(Json &&x) {
    _type = x._type;
    _str = click_move(x._str);
    _cjson = x._cjson;
    x._cjson = 0;
    return *this;
}
#endif

/** @cond never */
template <typename U>
inline Json &Json::operator=(const Json_proxy_base<U> &x) {
    return *this = x.cvalue();
}
/** @endcond never */

inline Json &Json::operator++() {
    return *this += 1;
}
inline void Json::operator++(int) {
    ++(*this);
}
inline Json &Json::operator--() {
    return *this += -1;
}
inline void Json::operator--(int) {
    --(*this);
}
inline Json &Json::operator+=(int x) {
    force_number();
#if HAVE_FLOAT_TYPES
    if (_type == j_int)
	_str = String(as_i() + x);
    else
	_str = String(as_d() + x);
#else
    _str = String(as_i() + x);
#endif
    return *this;
}
inline Json &Json::operator+=(long x) {
    force_number();
#if HAVE_FLOAT_TYPES
    if (_type == j_int)
	_str = String(as_i() + x);
    else
	_str = String(as_d() + x);
#else
    _str = String(as_i() + x);
#endif
    return *this;
}
#if HAVE_FLOAT_TYPES
inline Json &Json::operator+=(double x) {
    force_number();
    _str = String(as_d() + x);
    return *this;
}
#endif

/** @brief Swap this Json with @a x. */
inline void Json::swap(Json &x) {
    click_swap(_type, x._type);
    _str.swap(x._str);
    click_swap(_cjson, x._cjson);
}


inline StringAccum &operator<<(StringAccum &sa, const Json &json) {
    json.unparse(sa);
    return sa;
}

inline bool operator==(const Json &a, const Json &b) {
    return a.type() == b.type()
	&& (a.is_primitive() ? a._str == b._str : a._cjson == b._cjson);
}

template <typename T>
inline bool operator==(const Json_proxy_base<T> &a, const Json &b) {
    return a.cvalue() == b;
}

template <typename T>
inline bool operator==(const Json &a, const Json_proxy_base<T> &b) {
    return a == b.cvalue();
}

template <typename T, typename U>
inline bool operator==(const Json_proxy_base<T> &a,
		       const Json_proxy_base<U> &b) {
    return a.cvalue() == b.cvalue();
}

inline bool operator!=(const Json &a, const Json &b) {
    return !(a == b);
}

template <typename T>
inline bool operator!=(const Json_proxy_base<T> &a, const Json &b) {
    return !(a == b);
}

template <typename T>
inline bool operator!=(const Json &a, const Json_proxy_base<T> &b) {
    return !(a == b);
}

template <typename T, typename U>
inline bool operator!=(const Json_proxy_base<T> &a,
		       const Json_proxy_base<U> &b) {
    return !(a == b);
}

CLICK_ENDDECLS
#endif
