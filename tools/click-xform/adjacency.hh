#ifndef ADJACENCY_HH
#define ADJACENCY_HH
#include "vector.hh"
class RouterT;

class AdjacencyMatrix {

  unsigned *_x;
  int _n;
  int _cap;
  Vector<int> _default_match;

  AdjacencyMatrix(const AdjacencyMatrix &);
  AdjacencyMatrix &operator=(const AdjacencyMatrix &);
  
 public:

  AdjacencyMatrix(RouterT *, RouterT * = 0);
  ~AdjacencyMatrix();

  void init(RouterT *, RouterT * = 0);
  void update(RouterT *, const Vector<int> &changed_eindices);
  void print() const;

  bool next_subgraph_isomorphism(const AdjacencyMatrix *, Vector<int> &) const;
  
};

bool check_subgraph_isomorphism(const RouterT *, const RouterT *, const Vector<int> &);

#endif
