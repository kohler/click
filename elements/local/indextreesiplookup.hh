#ifndef CLICK_INDEXTREEIPLOOKUP_HH
#define CLICK_INDEXTREEIPLOOKUP_HH
#include <click/glue.hh>
#include <click/element.hh>
#include "../ip/iproutetable.hh"
CLICK_DECLS

/*
 * =c
 * IndexTreesIPLookup()
 * =s iproute
 * IP lookup using an index tree
 * =d
 *
 * Performs IP lookup using an index of trees, using the IPRouteTable
 * interface. See IPRouteTable for description. Implementation
 * inspired by a similar implementation in the plan9 operating system
 * from bell labs (plan9.bell-labs.com).
 *
 * =a IPRouteTable
 */

class TreeNode {
public:
  TreeNode(IPAddress, IPAddress, IPAddress, unsigned);
  ~TreeNode() CLICK_COLD;

  void insert(IPAddress, IPAddress, IPAddress, unsigned);
  void remove(IPAddress, IPAddress);
  TreeNode* search(IPAddress);

  IPAddress dst() const		{ return _dst; }
  IPAddress mask() const	{ return _mask; }
  IPAddress gw() const		{ return _gw; }
  unsigned port() const		{ return _port; }

private:
  TreeNode *_left;
  TreeNode *_right;
  TreeNode *_middle;

  IPAddress _dst;
  IPAddress _mask;
  IPAddress _gw;
  unsigned  _port;
};

class IndexTreesIPLookup : public Element {
public:
  IndexTreesIPLookup() CLICK_COLD;
  ~IndexTreesIPLookup() CLICK_COLD;

  const char *class_name() const	{ return "IndexTreesIPLookup"; }
  const char *port_count() const	{ return PORTS_1_1; }
  void cleanup(CleanupStage) CLICK_COLD;

  String dump_routes();
  void add_route(IPAddress, IPAddress, IPAddress, int);
  void remove_route(IPAddress, IPAddress);
  int lookup_route(IPAddress, IPAddress &);

private:
  TreeNode* _trees[INDEX_SIZE];

  static const int INDEX_SIZE = 256;
  static int hash(unsigned a) { return ((a>>24)&255); }
};

CLICK_ENDDECLS
#endif
