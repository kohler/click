// -*- c-basic-offset: 4; related-file-name: "../../lib/ipaddresslist.cc" -*-
#ifndef CLICK_IPADDRESSLIST_HH
#define CLICK_IPADDRESSLIST_HH
#include <click/ipaddress.hh>
#include <click/vector.hh>
CLICK_DECLS

class IPAddressList { public:

    IPAddressList()			: _n(0), _v(0) { }
    ~IPAddressList()			{ delete[] _v; }
    
    bool empty() const			{ return _n == 0; }
    int size() const			{ return _n; }

    void push_back(IPAddress);
    void insert(IPAddress);
    void assign(int, uint32_t *);
    
    bool contains(IPAddress) const;

    void sort();
    bool binsearch_contains(IPAddress) const;
    
  private:

    int _n;
    uint32_t *_v;

};

inline void
IPAddressList::assign(int n, uint32_t *v)
{
    delete[] _v;
    _n = n;
    _v = v;
}

inline bool
IPAddressList::contains(IPAddress a) const
{
    for (int i = 0; i < _n; i++)
	if (_v[i] == a.addr())
	    return true;
    return false;
}

inline bool
IPAddressList::binsearch_contains(IPAddress a) const
{
    int l = 0, r = _n - 1;
    while (l <= r) {
	int m = (l + r) / 2;
	int32_t diff = a.addr() - _v[m];
	if (diff == 0)
	    return true;
	else if (diff < 0)
	    r = m - 1;
	else
	    l = m + 1;
    }
    return false;
}

CLICK_ENDDECLS
#endif
