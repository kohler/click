// -*- c-basic-offset: 2; related-file-name: "../../lib/ipaddressset.cc" -*-
#ifndef CLICK_IPADDRESSSET_HH
#define CLICK_IPADDRESSSET_HH
#include <click/ipaddress.hh>
#include <click/vector.hh>
CLICK_DECLS

class IPAddressSet { public:

  IPAddressSet()			{ }

  bool empty() const			{ return _s.size() == 0; }
  int size() const			{ return _s.size(); }
  
  void insert(IPAddress);
  void clear()				{ _s.clear(); }

  bool find(IPAddress) const;
  bool contains(IPAddress ip) const	{ return find(ip); }

  unsigned *list_copy();
  
 private:

  Vector<uint32_t> _s;

};

CLICK_ENDDECLS
#endif
