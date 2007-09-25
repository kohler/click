#ifndef CLICKY_RECTSEARCH_HH
#define CLICKY_RECTSEARCH_HH 1
#include <list>
#include <vector>
#include <algorithm>
#include <click/hashmap.hh>
#include <click/hashmap.cc>
#include <math.h>

template <typename T, int CHUNK = 1024>
class rect_search { public:

    rect_search();

    void clear();
    void insert(T *v);
    void erase(T *v);
    void find_all(double x, double y, std::vector<T *> &v) const;
    void find_all(const rectangle &r, std::vector<T *> &v) const;

  private:

    typedef HashMap<int, std::list<T * > > rectmap;
    rectmap _stuff;

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
    assert(v->width() < 1000 * CHUNK && v->height() < 1000 * CHUNK);
    int xi1 = (int) floor(v->x() / CHUNK);
    int yi1 = (int) floor(v->y() / CHUNK);
    double x1 = CHUNK * xi1;
    double y1 = CHUNK * yi1;
    double x2 = v->x2();
    double y2 = v->y2();
    for (int i = 0; x1 + i * CHUNK < x2; ++i)
	for (int j = 0; y1 + j * CHUNK < y2; ++j) {
	    int nnn = (((xi1 + i) & 0xFFF) << 12) + ((yi1 + j) & 0xFFF);
	    std::list<T *> &l = _stuff.find_force(nnn);
	    l.push_front(v);
	}
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::erase(T *v)
{
    assert(v->width() < 1000 * CHUNK && v->height() < 1000 * CHUNK);
    int xi1 = (int) floor(v->x() / CHUNK);
    int yi1 = (int) floor(v->y() / CHUNK);
    double x1 = CHUNK * xi1;
    double y1 = CHUNK * yi1;
    double x2 = v->x2();
    double y2 = v->y2();
    for (int i = 0; x1 + i * CHUNK < x2; ++i)
	for (int j = 0; y1 + j * CHUNK < y2; ++j) {
	    int nnn = (((xi1 + i) & 0xFFF) << 12) + ((yi1 + j) & 0xFFF);
	    std::list<T *> &l = _stuff.find_force(nnn);
	    std::remove(l.begin(), l.end(), v);
	}
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::find_all(double x, double y, std::vector<T *> &results) const
{
    int i = (int) floor(x / CHUNK);
    int j = (int) floor(y / CHUNK);
    int nnn = ((i & 0xFFF) << 12) + (j & 0xFFF);
    std::list<T *> *l = _stuff.findp(nnn);
    if (l) {
	for (typename std::list<T *>::iterator iter = l->begin();
	     iter != l->end();
	     ++iter) {
	    T *v = *iter;
	    if (v->contains(x, y))
		results.push_back(v);
	}
    }
}

template <typename T, int CHUNK>
void rect_search<T, CHUNK>::find_all(const rectangle &r, std::vector<T *> &results) const
{
    // if the rectangle is really big, just look at all containers
    if ((r.width() / CHUNK) > (_stuff.size() * (double) CHUNK) / r.height()) {
	for (typename rectmap::const_iterator hiter = _stuff.begin();
	     hiter != _stuff.end(); ++hiter)
	    for (typename std::list<T *>::const_iterator iter = hiter.value().begin();
		 iter != hiter.value().end(); ++iter) {
		T *v = *iter;
		if (*v & r)
		    results.push_back(v);
	    }
	return;
    }
    
    int xi1 = (int) floor(r.x() / CHUNK);
    int yi1 = (int) floor(r.y() / CHUNK);
    double x1 = CHUNK * xi1;
    double y1 = CHUNK * yi1;
    for (int i = 0; x1 + i * CHUNK < r.x2(); ++i)
	for (int j = 0; y1 + j * CHUNK < r.y2(); ++j) {
	    int nnn = (((xi1 + i) & 0xFFF) << 12) + ((yi1 + j) & 0xFFF);
	    std::list<T *> *l = _stuff.findp(nnn);
	    if (l) {
		for (typename std::list<T *>::iterator iter = l->begin();
		     iter != l->end(); ++iter) {
		    T *v = *iter;
		    if (*v & r)
			results.push_back(v);
		}
	    }
	}
}

#endif
