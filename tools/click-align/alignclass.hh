#ifndef ALIGNCLASS_HH
#define ALIGNCLASS_HH
#include "alignment.hh"
#include "elementt.hh"

class Aligner {
 public:
  Aligner();
  virtual void flow(const Vector<Alignment> &in, int ioff, int nin,
		    Vector<Alignment> &out, int ooff, int nout);
  virtual Alignment want(int);
};

class GeneratorAligner : public Aligner {
  Alignment _alignment;
 public:
  GeneratorAligner(const Alignment &a) : _alignment(a) { }
  void flow(const Vector<Alignment> &in, int ioff, int nin,
	    Vector<Alignment> &out, int ooff, int nout);
};

class ShifterAligner : public Aligner {
  int _shift;
 public:
  ShifterAligner(int shift) : _shift(shift) { }
  void flow(const Vector<Alignment> &in, int ioff, int nin,
	    Vector<Alignment> &out, int ooff, int nout);
};

class WantAligner : public Aligner {
  Alignment _alignment;
 public:
  WantAligner(Alignment a) : _alignment(a) { }
  Alignment want(int);
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
