#ifndef IPADDRESSSET_HH
#define IPADDRESSSET_HH
#include "ipaddress.hh"
#include "vector.hh"

class IPAddressSet {

  Vector<unsigned> _s;

 public:

  IPAddressSet()			{ }

  bool empty() const			{ return _s.size() == 0; }
  int size() const			{ return _s.size(); }
  
  void insert(IPAddress);
  void clear()				{ _s.clear(); }

  bool find(IPAddress) const;
  bool contains(IPAddress ip) const	{ return find(ip); }

  unsigned *list_copy();
  
};

#endif
