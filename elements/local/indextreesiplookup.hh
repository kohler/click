#ifndef INDEXTREEIPLOOKUP_HH
#define INDEXTREEIPLOOKUP_HH

/*
 * =c
 * IndexTreesIPLookup()
 * =s IP, classification
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

#include <click/glue.hh>
#include <click/element.hh>
#include "../ip/iproutetable.hh"

class TreeNode {
public:
  TreeNode(IPAddress, IPAddress, IPAddress, unsigned);
  ~TreeNode();

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
  IndexTreesIPLookup();
  ~IndexTreesIPLookup();
  
  const char *class_name() const	{ return "IndexTreesIPLookup"; }
  const char *processing() const	{ return AGNOSTIC; }
  IndexTreesIPLookup *clone() const	{ return new IndexTreesIPLookup; }
  void cleanup(CleanupStage);

  String dump_routes();
  void add_route(IPAddress, IPAddress, IPAddress, int);
  void remove_route(IPAddress, IPAddress);
  int lookup_route(IPAddress, IPAddress &);

private:
  TreeNode* _trees[INDEX_SIZE];
  
  static const int INDEX_SIZE = 256;
  static int hash(unsigned a) { return ((a>>24)&255); }
};


#endif

