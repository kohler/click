#ifndef CLICK_PROCESSINGT_HH
#define CLICK_PROCESSINGT_HH
#include "routert.hh"
class ElementMap;
class Bitvector;

class ProcessingT { public:

  enum ProcessingCode { VAGNOSTIC = 0, VPUSH = 1, VPULL = 2 };

  ProcessingT();
  ProcessingT(const RouterT *, ErrorHandler *);
  ProcessingT(const RouterT *, ElementMap *, ErrorHandler *);

  int nelements() const		{ return _input_pidx.size() - 1; }
  int ninput_pidx() const	{ return _input_pidx.back(); }
  int noutput_pidx() const	{ return _output_pidx.back(); }
  
  int input_pidx(int ei, int p = 0) const	{ return _input_pidx[ei]+p; }
  int output_pidx(int ei, int p = 0) const	{ return _output_pidx[ei]+p; }
  int input_pidx(const HookupI &h) const;
  int output_pidx(const HookupI &h) const;
    
  int ninputs(int i) const	{ return _input_pidx[i+1] - _input_pidx[i]; }
  int noutputs(int i) const	{ return _output_pidx[i+1] - _output_pidx[i]; }

  int input_processing(int i, int p) const;
  int output_processing(int i, int p) const;
  bool input_is_pull(int i, int p) const;
  bool output_is_push(int i, int p) const;
  const HookupI &input_connection(int i, int p) const;
  const HookupI &output_connection(int i, int p) const;

  bool is_internal_flow(int i, int ip, int op) const;
  
  int reset(const RouterT *, ErrorHandler *);

  bool same_processing(int, int) const;

  static int forward_flow(const String &code, int input_port, int noutputs, Bitvector *);
  static int backward_flow(const String &code, int output_port, int ninputs, Bitvector *);
  
 private:

  const RouterT *_router;
  
  Vector<int> _input_pidx;
  Vector<int> _output_pidx;
  Vector<int> _input_eidx;
  Vector<int> _output_eidx;
  Vector<int> _input_processing;
  Vector<int> _output_processing;
  Vector<HookupI> _connected_input;
  Vector<HookupI> _connected_output;

  void create_pidx(ErrorHandler *);
  
  void initial_processing_for(int, ErrorHandler *);
  void initial_processing(ErrorHandler *);
  void processing_error(const HookupI &, const HookupI &, int, int,
			ErrorHandler *);
  void check_processing(ErrorHandler *);
  void check_connections(ErrorHandler *);
  
};


inline int
ProcessingT::input_pidx(const HookupI &h) const
{
  return input_pidx(h.idx, h.port);
}

inline int
ProcessingT::output_pidx(const HookupI &h) const
{
  return output_pidx(h.idx, h.port);
}

inline int
ProcessingT::input_processing(int i, int p) const
{
  return _input_processing[input_pidx(i, p)];
}

inline int
ProcessingT::output_processing(int i, int p) const
{
  return _output_processing[output_pidx(i, p)];
}

inline bool
ProcessingT::input_is_pull(int i, int p) const
{
  return input_processing(i, p) == VPULL;
}

inline bool
ProcessingT::output_is_push(int i, int p) const
{
  return output_processing(i, p) == VPUSH;
}

inline const HookupI &
ProcessingT::input_connection(int i, int p) const
{
  return _connected_input[input_pidx(i, p)];
}

inline const HookupI &
ProcessingT::output_connection(int i, int p) const
{
  return _connected_output[output_pidx(i, p)];
}

#endif
