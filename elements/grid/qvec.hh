// qvec.hh
// Douglas S. J. De Couto
// 5 March 2003

// Poor man's queue

#ifndef CLICK_QUEUE_HH
#define CLICK_QUEUE_HH

#include <click/vector.hh>

CLICK_DECLS

template <class T>
class QVec : public Vector<T> {
  typedef Vector<T> Base;
public:
  QVec() { }
  explicit QVec(int capacity) : Base(capacity) { }

  T &front() { return at(0); }

  void push_front(const T &);
  void pop_front();
};



template <class T> inline void
QVec<T>::push_front(const T &e)
{
  int n = size();
  if (n > 0)
    push_back(back());
  for (int i = 1; i < n; i++)
    this->at(i) = this->at(i - 1);
  this->at(0) = e;
}

template <class T> inline void
QVec<T>::pop_front()
{
  assert(size() > 0);
  int n = size();
  for (int i = 0; i < n - 1; i++)
    this->at(i) = this->at(i + 1);
  pop_back();
}


CLICK_ENDDECLS
#endif
