#ifndef CLICKY_RECTSEARCH_HH
#define CLICKY_RECTSEARCH_HH 1
#include <list>
#include <vector>
#include <algorithm>
#include <click/hashtable.hh>
#include <math.h>

template <typename T, int CHUNK = 256>
class rect_search { public:

    rect_search();

    void clear();
    void insert(T *v);
    void remove(T *v);
    void find_all(double x, double y, std::vector<T *> &v) const;
    void find_all(const rectangle &r, std::vector<T *> &v) const;

  private:

    typedef HashTable<int, std::list<T *> > rectmap;
    rectmap _stuff;
    std::list<T *> _big_stuff;

    static void find_some(const std::list<T *> &l, double x, double y, std::vector<T *> &v);
    static void find_some(const std::list<T *> &l, const rectangle &r, std::vector<T *> &v);

};

template <typename T, int CHUNK>
rect_search<T, CHUNK>::rect_search()
{
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::clear()
{
    _stuff.clear();
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::insert(T *v)
{
    if (v->width() * v->height() >= 50 * CHUNK * CHUNK)
	_big_stuff.push_front(v);
    else {
	int xi1 = (int) floor(v->x() / CHUNK);
	int yi1 = (int) floor(v->y() / CHUNK);
	double x1 = CHUNK * xi1;
	double y1 = CHUNK * yi1;
	double x2 = v->x2();
	double y2 = v->y2();
	for (int i = 0; x1 + i * CHUNK < x2; ++i)
	    for (int j = 0; y1 + j * CHUNK < y2; ++j) {
		int nnn = (((xi1 + i) & 0xFFF) << 12) + ((yi1 + j) & 0xFFF);
		typename rectmap::iterator it = _stuff.find_insert(nnn);
		it.value().push_front(v);
	    }
    }
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::remove(T *v)
{
    if (v->width() * v->height() >= 50 * CHUNK * CHUNK)
	_big_stuff.remove(v);
    else {
	int xi1 = (int) floor(v->x() / CHUNK);
	int yi1 = (int) floor(v->y() / CHUNK);
	double x1 = CHUNK * xi1;
	double y1 = CHUNK * yi1;
	double x2 = v->x2();
	double y2 = v->y2();
	for (int i = 0; x1 + i * CHUNK < x2; ++i)
	    for (int j = 0; y1 + j * CHUNK < y2; ++j) {
		int nnn = (((xi1 + i) & 0xFFF) << 12) + ((yi1 + j) & 0xFFF);
		typename rectmap::iterator it = _stuff.find_insert(nnn);
		it.value().remove(v);
	    }
    }
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::find_some(const std::list<T *> &l, double x, double y, std::vector<T *> &result)
{
    for (typename std::list<T *>::const_iterator iter = l.begin();
	 iter != l.end(); ++iter)
	if ((*iter)->contains(x, y))
	    result.push_back(*iter);
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::find_some(const std::list<T *> &l, const rectangle &r, std::vector<T *> &result)
{
    for (typename std::list<T *>::const_iterator iter = l.begin();
	 iter != l.end(); ++iter)
	if (**iter & r)
	    result.push_back(*iter);
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::find_all(double x, double y, std::vector<T *> &result) const
{
    int i = (int) floor(x / CHUNK);
    int j = (int) floor(y / CHUNK);
    int nnn = ((i & 0xFFF) << 12) + (j & 0xFFF);
    if (typename rectmap::const_iterator it = _stuff.find(nnn))
	find_some(it.value(), x, y, result);
    find_some(_big_stuff, x, y, result);
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::find_all(const rectangle &r, std::vector<T *> &result) const
{
    // if the rectangle is really big, just look at all containers
    if ((r.width() / CHUNK) > (_stuff.size() * (double) CHUNK) / r.height()) {
	for (typename rectmap::const_iterator hiter = _stuff.begin();
	     hiter != _stuff.end(); ++hiter)
	    find_some(hiter.value(), r, result);
    } else {
	int xi1 = (int) floor(r.x() / CHUNK);
	int yi1 = (int) floor(r.y() / CHUNK);
	double x1 = CHUNK * xi1;
	double y1 = CHUNK * yi1;
	for (int i = 0; x1 + i * CHUNK < r.x2(); ++i)
	    for (int j = 0; y1 + j * CHUNK < r.y2(); ++j) {
		int nnn = (((xi1 + i) & 0xFFF) << 12) + ((yi1 + j) & 0xFFF);
		if (typename rectmap::const_iterator it = _stuff.find(nnn))
		    find_some(it.value(), r, result);
	    }
    }
    find_some(_big_stuff, r, result);
}

#if 0
template <typename T, int CHUNK>
void rect_search<T, CHUNK>::print_sizes()
{
    for (typename rectmap::const_iterator hiter = _stuff.begin();
	 hiter != _stuff.end(); ++hiter)
	fprintf(stderr, "%d ", hiter->second.size());
    fprintf(stderr, "%d\n", _big_stuff.size());
}
#endif

#endif
