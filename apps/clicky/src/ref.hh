#ifndef TAMER_REF_HH
#define TAMER_REF_HH 1
/* Copyright (c) 2007, Eddie Kohler
 * Copyright (c) 2007, Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Tamer LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Tamer LICENSE file; the license in that file is
 * legally binding.
 */

template <typename T> class ref_ptr;

class enable_ref_ptr { public:

    enable_ref_ptr()
	: _use_count(1) {
    }

    ~enable_ref_ptr() {
	assert(!_use_count);
    }

  private:

    uint32_t _use_count;

    enable_ref_ptr(const enable_ref_ptr &);
    enable_ref_ptr &operator=(const enable_ref_ptr &);

    template <typename T> friend class ref_ptr;

    void add_ref_copy() {
	++_use_count;
    }

    bool release() {
	assert(_use_count);
	return (--_use_count == 0);
    }

    uint32_t use_count() const {
	return _use_count;
    }

};


template <typename T> class ref_ptr { public:

    ref_ptr()
	: _t(0) {
    }

    template <typename U> explicit ref_ptr(U *u)
	: _t(u) {
	assert(!_t || _t->use_count() == 1);
    }

    ref_ptr(const ref_ptr<T> &r)
	: _t(r._t) {
	if (_t)
	    _t->add_ref_copy();
    }

    template <typename U> ref_ptr(const ref_ptr<U> &r)
	: _t(r._t) {
	if (_t)
	    _t->add_ref_copy();
    }

    ~ref_ptr() {
	if (_t && _t->release())
	    delete _t;
    }

    ref_ptr<T> &operator=(const ref_ptr<T> &r) {
	if (r._t)
	    r._t->add_ref_copy();
	if (_t && _t->release())
	    delete _t;
	_t = r._t;
	return *this;
    }

    template <typename U> ref_ptr<T> &operator=(const ref_ptr<U> &r) {
	if (r._t)
	    r._t->add_ref_copy();
	if (_t && _t->release())
	    delete _t;
	_t = r._t;
	return *this;
    }

    T &operator*() const {
	return *_t;
    }

    T *operator->() const {
	return _t;
    }

    T *get() const {
	return _t;
    }

    typedef T *(ref_ptr::*unspecified_bool_type)() const;

    operator unspecified_bool_type() const {
	return _t ? &ref_ptr::get : 0;
    }

    bool operator!() const {
	return !_t;
    }

  private:

    T *_t;

};

template <typename T, typename U>
inline bool operator==(const ref_ptr<T> &a, const ref_ptr<U> &b)
{
    return a.get() == b.get();
}

template <typename T, typename U>
inline bool operator!=(const ref_ptr<T> &a, const ref_ptr<U> &b)
{
    return a.get() != b.get();
}

#endif /* TAMER_REF_HH */
