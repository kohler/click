#ifndef CLICK_XFORM_ADJACENCY_HH
#define CLICK_XFORM_ADJACENCY_HH
#include <click/vector.hh>
class RouterT;
class ElementT;

class AdjacencyMatrix {

  unsigned *_x;
  int _n;
  int _cap;
  Vector<int> _default_match;
  mutable Vector<int> _output_0_of;
  RouterT *_router;

  AdjacencyMatrix(const AdjacencyMatrix &);
  AdjacencyMatrix &operator=(const AdjacencyMatrix &);

  void init_pattern() const;

 public:

  AdjacencyMatrix(RouterT *);
  ~AdjacencyMatrix();

  void init(RouterT *);
  void update(const Vector<int> &changed_eindices);
  void print() const;

  bool connection_exists(int i, int j) const;

  bool next_subgraph_isomorphism(const AdjacencyMatrix *, Vector<ElementT *> &) const;

};

bool check_subgraph_isomorphism(const RouterT *, const RouterT *, const Vector<ElementT *> &);

inline bool
AdjacencyMatrix::connection_exists(int i, int j) const
{
  return _x[ i + (j<<_cap) ] & 1;
}

#endif
