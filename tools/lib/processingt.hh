// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PROCESSINGT_HH
#define CLICK_PROCESSINGT_HH
#include "routert.hh"
class ElementMap;
class Bitvector;

class ProcessingT { public:

    enum ProcessingCode { VAGNOSTIC = 0, VPUSH = 1, VPULL = 2 };
    static const char * const processing_letters;

    ProcessingT();
    ProcessingT(const RouterT *, ErrorHandler *);
    ProcessingT(const RouterT *, ElementMap *, ErrorHandler *);
    ProcessingT(const RouterT *, ElementMap *, bool flatten, ErrorHandler *);
    int reset(const RouterT *, ElementMap *, bool flatten, ErrorHandler *);
    void resolve_agnostics();	// change remaining AGNOSTICs to PUSH

    int nelements() const	{ return _input_pidx.size() - 1; }
    int ninput_pidx() const	{ return _input_pidx.back(); }
    int noutput_pidx() const	{ return _output_pidx.back(); }

    int input_pidx(const ConnectionT &) const;
    int output_pidx(const ConnectionT &) const;
    int input_pidx(const PortT &) const;
    int output_pidx(const PortT &) const;
    int input_pidx(int ei, int p = 0) const	{ return _input_pidx[ei]+p; }
    int output_pidx(int ei, int p = 0) const	{ return _output_pidx[ei]+p; }
    PortT input_port(int pidx) const;
    PortT output_port(int pidx) const;

    int input_processing(const PortT &) const;
    int output_processing(const PortT &) const;
    int input_processing(int ei, int p) const;
    int output_processing(int ei, int p) const;
    bool input_is_pull(int ei, int p) const;
    bool output_is_push(int ei, int p) const;
    const PortT &input_connection(int ei, int p) const;
    const PortT &output_connection(int ei, int p) const;

    bool same_processing(int, int) const;

    static int forward_flow(const String &code, int input_port, int noutputs, Bitvector *, ErrorHandler * = 0);
    static int forward_flow(const PortT &, Bitvector *, ErrorHandler * = 0);
    static int backward_flow(const String &code, int output_port, int ninputs, Bitvector *, ErrorHandler * = 0);
    static int backward_flow(const PortT &, Bitvector *, ErrorHandler * = 0);

    void set_connected_inputs(const Bitvector &, Bitvector &) const;
    void set_connected_outputs(const Bitvector &, Bitvector &) const;
    void set_connected_inputs(const PortT &, Bitvector &) const;
    void set_connected_outputs(const PortT &, Bitvector &) const;
    void set_flowed_inputs(const Bitvector &, Bitvector &, ErrorHandler* = 0) const;
    void set_flowed_outputs(const Bitvector &, Bitvector &, ErrorHandler* = 0) const;

    void forward_reachable_inputs(Bitvector &, ErrorHandler * = 0) const;

    String compound_processing_code() const;
    String compound_flow_code(ErrorHandler * = 0) const;
    
  private:

    const RouterT *_router;

    Vector<int> _input_pidx;
    Vector<int> _output_pidx;
    Vector<const ElementT *> _input_elt;
    Vector<const ElementT *> _output_elt;
    Vector<int> _input_processing;
    Vector<int> _output_processing;
    Vector<PortT> _connected_input;
    Vector<PortT> _connected_output;

    void create_pidx(ErrorHandler *);

    void initial_processing_for(int, ErrorHandler *);
    void initial_processing(ErrorHandler *);
    void processing_error(const ConnectionT &, int, ErrorHandler *);
    void check_processing(ErrorHandler *);
    void check_connections(ErrorHandler *);

};


inline int
ProcessingT::input_pidx(const PortT &h) const
{
    assert(h.router() == _router);
    return input_pidx(h.idx(), h.port);
}

inline int
ProcessingT::output_pidx(const PortT &h) const
{
    assert(h.router() == _router);
    return output_pidx(h.idx(), h.port);
}

inline int
ProcessingT::input_pidx(const ConnectionT &c) const
{
    return input_pidx(c.to());
}

inline int
ProcessingT::output_pidx(const ConnectionT &c) const
{
    return output_pidx(c.from());
}

inline PortT
ProcessingT::input_port(int pidx) const
{
    const ElementT *elt = _input_elt[pidx];
    return PortT(const_cast<ElementT *>(elt), pidx - _input_pidx[elt->idx()]);
}

inline PortT
ProcessingT::output_port(int pidx) const
{
    const ElementT *elt = _output_elt[pidx];
    return PortT(const_cast<ElementT *>(elt), pidx - _output_pidx[elt->idx()]);
}

inline int
ProcessingT::input_processing(const PortT &h) const
{
    return _input_processing[input_pidx(h)];
}

inline int
ProcessingT::output_processing(const PortT &h) const
{
    return _output_processing[output_pidx(h)];
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

inline const PortT &
ProcessingT::input_connection(int i, int p) const
{
    return _connected_input[input_pidx(i, p)];
}

inline const PortT &
ProcessingT::output_connection(int i, int p) const
{
    return _connected_output[output_pidx(i, p)];
}

inline int
ProcessingT::forward_flow(const PortT &p, Bitvector *bv, ErrorHandler *errh)
{
    return forward_flow(p.elt->type()->flow_code(), p.port, p.elt->noutputs(), bv, errh);
}

inline int
ProcessingT::backward_flow(const PortT &p, Bitvector *bv, ErrorHandler *errh)
{
    return backward_flow(p.elt->type()->flow_code(), p.port, p.elt->ninputs(), bv, errh);
}

#endif
