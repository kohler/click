#ifndef ALIGNCLASS_HH
#define ALIGNCLASS_HH
#include "alignment.hh"
#include "elementt.hh"

class Aligner {
 public:
  Aligner();
  virtual void have_flow(const Vector<Alignment> &in, int ioff, int nin,
			 Vector<Alignment> &out, int ooff, int nout);
  virtual void want_flow(Vector<Alignment> &in, int ioff, int nin,
			 const Vector<Alignment> &out, int ooff, int nout);
};

class CombinedAligner : public Aligner {
  Aligner *_have;
  Aligner *_want;
 public:
  CombinedAligner(Aligner *have, Aligner *want) : _have(have), _want(want) { }
  void have_flow(const Vector<Alignment> &in, int ioff, int nin,
		 Vector<Alignment> &out, int ooff, int nout);
  void want_flow(Vector<Alignment> &in, int ioff, int nin,
		 const Vector<Alignment> &out, int ooff, int nout);
};

class GeneratorAligner : public Aligner {
  Alignment _alignment;
 public:
  GeneratorAligner(const Alignment &a) : _alignment(a) { }
  void have_flow(const Vector<Alignment> &in, int ioff, int nin,
		 Vector<Alignment> &out, int ooff, int nout);
  void want_flow(Vector<Alignment> &in, int ioff, int nin,
		 const Vector<Alignment> &out, int ooff, int nout);
};

class ShifterAligner : public Aligner {
  int _shift;
 public:
  ShifterAligner(int shift) : _shift(shift) { }
  void have_flow(const Vector<Alignment> &in, int ioff, int nin,
		 Vector<Alignment> &out, int ooff, int nout);
  void want_flow(Vector<Alignment> &in, int ioff, int nin,
		 const Vector<Alignment> &out, int ooff, int nout);
};

class WantAligner : public Aligner {
  Alignment _alignment;
 public:
  WantAligner(Alignment a) : _alignment(a) { }
  void want_flow(Vector<Alignment> &in, int ioff, int nin,
		 const Vector<Alignment> &out, int ooff, int nout);
};

class ClassifierAligner : public Aligner {
 public:
  ClassifierAligner() { }
  void want_flow(Vector<Alignment> &in, int ioff, int nin,
		 const Vector<Alignment> &out, int ooff, int nout);
};


class AlignClass : public ElementClassT {
  Aligner *_aligner;
 public:
  AlignClass();
  AlignClass(Aligner *);
  virtual Aligner *create_aligner(ElementT &, RouterT *, ErrorHandler *);
};

class StripAlignClass : public AlignClass {
 public:
  StripAlignClass();
  Aligner *create_aligner(ElementT &, RouterT *, ErrorHandler *);
};

class AlignAlignClass : public AlignClass {
 public:
  AlignAlignClass();
  Aligner *create_aligner(ElementT &, RouterT *, ErrorHandler *);
};


Alignment common_alignment(const Vector<Alignment> &, int off, int count);
Aligner *default_aligner();

#endif
