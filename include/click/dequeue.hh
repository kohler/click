// dequeue.hh
// Douglas S. J. De Couto
// 26 June 2003

// Inspired by Click Vector<> and SimpleQueue code

#ifndef CLICK_DEQUEUE_HH
#define CLICK_DEQUEUE_HH
#include <click/glue.hh>
CLICK_DECLS

template <class T>
class DEQueue {

public:
  DEQueue()                      : _l(0), _n(0), _cap(0), _head(0), _tail(0) { }
  explicit DEQueue(int capacity) : _l(0), _n(0), _cap(0), _head(0), _tail(0) { reserve(capacity); }
  DEQueue(int n, const T &e)     : _l(0), _n(0), _cap(0), _head(0), _tail(0) { resize(n, e);      }
  DEQueue(const DEQueue<T> &);
  ~DEQueue();

  void check_rep();

  int size() const { return _n; }

  const T &at(int i) const		{ assert(i>=0 && i<_n); return _l[idx(i)]; }
  const T &operator[](int i) const	{ return at(i); }
  const T &front() const                { return at(0); }
  const T &back() const			{ return at(_n - 1); }
  const T &at_u(int i) const		{ return _l[idx(i)]; }
  
  T &at(int i)				{ assert(i>=0 && i<_n); return _l[idx(i)]; }
  T &operator[](int i)			{ return at(i); }
  T &front()                            { return at(0); }
  T &back()				{ return at(_n - 1); }
  T &at_u(int i)			{ return _l[idx(i)]; }

    struct const_iterator {
	const DEQueue &_q;
	int _pos;

	const_iterator(const DEQueue<T> &q, int p) : _q(q), _pos(p)  { }
	const_iterator(const const_iterator &i) : _q(i._q), _pos(i._pos) { }
	const_iterator &operator++() { _pos++; return *this; }
	const_iterator &operator--() { _pos--; return *this; }
	const_iterator operator++(int) { const_iterator t = *this; ++*this; return t; }
	const_iterator operator--(int) { const_iterator t = *this; --*this; return t; }
	const_iterator &operator+=(int n) { _pos += n; return *this; }
	const_iterator &operator-=(int n) { _pos -= n; return *this; }
	const T &operator[](int n) const { return _q[_pos + n]; }
	bool operator==(const const_iterator &i) const { return _pos == i._pos && _q._l == i._q._l;  }
	bool operator!=(const const_iterator &i) const { return _pos != i._pos || _q._l != i._q._l;  }
	const T &operator*()  const { return _q[_pos];  }
	const T *operator->() const { return &_q[_pos]; }
    };

    struct iterator : public const_iterator {
	iterator(DEQueue<T> &q, int p) : const_iterator(q, p) { }
	iterator(const iterator &i) : const_iterator(i._q, i._pos) { }
	T &operator[](int n) const { return const_cast<T &>(const_iterator::operator[](n)); }
	T &operator*() const { return const_cast<T &>(const_iterator::operator*()); }
	T *operator->() const { return const_cast<T *>(const_iterator::operator->()); }
    };
  
    iterator begin()			{ return iterator(*this, 0); }
    const_iterator begin() const	{ return const_iterator((DEQueue<T> &) *this, 0); }
    iterator end()			{ return iterator(*this, _n); }
    const_iterator end() const		{ return const_iterator((DEQueue<T> &) *this, _n); }

  void push_back(const T &);
  void pop_back();
  
  void push_front(const T &);
  void pop_front();

  void clear()                          { shrink(0); }
  bool reserve(int);
  void resize(int nn, const T &e = T());

  DEQueue<T> &operator=(const DEQueue<T> &);
  DEQueue<T> &assign(int n, const T &e = T());
  void swap(DEQueue<T> &);

private:

  T *_l;
  int _n;
  int _cap;
  int _head;
  int _tail;

  int next_i(int i) const               { return (i != _cap - 1 ? i + 1 : 0); }
  int prev_i(int i) const               { return (i != 0 ? i - 1 : _cap - 1); }
  int idx(int i)    const               { return (i + _head) % _cap;  }

  void *velt(int i) const		{ return (void *) &_l[i]; }
  static void *velt(T *l, int i)	{ return (void *) &l[i];  }
  void shrink(int nn);
  
  friend class iterator;
  friend class const_iterator;

};



template <class T> inline void
DEQueue<T>::push_front(const T &e)
{
    if (_n < _cap || reserve(-1)) {
	_head = prev_i(_head);
	new(velt(_head)) T(e);
	_n++;
    }
}

template <class T> inline void
DEQueue<T>::pop_front()
{
    assert(_n > 0);
    --_n;
    _l[_head].~T();
    _head = next_i(_head);
}

template <class T> inline void
DEQueue<T>::push_back(const T &e)
{
    if (_n < _cap || reserve(-1)) {
	new(velt(_tail)) T(e);
	_n++;
	_tail = next_i(_tail);
    }
}

template <class T> inline void
DEQueue<T>::pop_back()
{
    assert(_n > 0);
    --_n;
    _tail = prev_i(_tail);
    _l[_tail].~T();
}

CLICK_ENDDECLS
#endif
