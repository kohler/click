#ifndef SIGNATURE_HH
#define SIGNATURE_HH
#include <click/vector.hh>
#include <click/string.hh>
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

  enum { SIG_NOT_SPECIAL = 0 };

  Signatures(const RouterT *);

  void specialize_class(const String &, bool);

  void analyze(ElementMap &);

  const Vector<int> &signature_ids() const	{ return _sigid; }
  int nsignatures() const			{ return _sigs.size(); }

 private:

  const RouterT *_router;

  Vector<int> _sigid;
  Vector<SignatureNode> _sigs;

  void create_phase_0(const ProcessingT &);
  void check_port_numbers(int eid, const ProcessingT &);
  bool next_phase(int phase, int eid, Vector<int> &, const ProcessingT &);
  void print_signature() const;

};

#endif
