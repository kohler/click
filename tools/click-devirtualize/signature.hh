#ifndef SIGNATURE_HH
#define SIGNATURE_HH
#include "vector.hh"
#include "string.hh"
class RouterT;
class ElementMap;
class ProcessingT;

struct SignatureNode {

  int _eid;
  int _phase;
  Vector<int> _connections;
  int _next;

  SignatureNode()			{ }
  SignatureNode(int eid)		: _eid(eid), _phase(0), _next(-1) { }
  
};

class Signatures { public:

  static const int SIG_NOT_SPECIAL = 0;

 private:
  
  const RouterT *_router;
  
  Vector<int> _sigid;
  Vector<SignatureNode> _sigs;

  void create_phase_0(const ProcessingT &);
  void check_port_numbers(int eid, const ProcessingT &);
  bool next_phase(int phase, int eid, Vector<int> &, const ProcessingT &);
  void print_signature() const;

 public:

  Signatures(const RouterT *);

  void specialize_class(const String &, bool);
  
  void analyze(const ElementMap &);

  const Vector<int> &signature_ids() const	{ return _sigid; }
  int nsignatures() const			{ return _sigs.size(); }
  
};

#endif
