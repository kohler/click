// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PROCESSINGT_HH
#define CLICK_PROCESSINGT_HH
#include "routert.hh"
class ElementMap;
class Bitvector;

class ProcessingT { public:

    enum ProcessingCode { VAGNOSTIC = 0, VPUSH = 1, VPULL = 2, VAFLAG = 4 };
    static const char processing_letters[];
    static const char decorated_processing_letters[];

    ProcessingT(const RouterT *, ElementMap *);
    void check_types(ErrorHandler *errh = 0);
    inline void check(ErrorHandler *errh = 0);
    void create(const String &compound_pcode, bool agnostic_to_push, ErrorHandler *errh = 0);

    int nelements() const	{ return _input_pidx.size() - 1; }
    int ninput_pidx() const	{ return _input_pidx.back(); }
    int noutput_pidx() const	{ return _output_pidx.back(); }
    ElementMap *element_map() const { return _element_map; }

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
    char decorated_input_processing_letter(const PortT &) const;
    char decorated_output_processing_letter(const PortT &) const;
    int input_processing(int ei, int p) const;
    int output_processing(int ei, int p) const;
    bool input_is_pull(int ei, int p) const;
    bool output_is_push(int ei, int p) const;
    const PortT &input_connection(int ei, int p) const;
    const PortT &output_connection(int ei, int p) const;

    bool same_processing(int, int) const;

    String processing_code(const ElementT *) const;
    String decorated_processing_code(const ElementT *) const;

    static const char *processing_code_next(const char *pos, const char *end_code, int &processing);
    static const char *processing_code_output(const char *code, const char *end_code, const char *pos = 0);
    static String processing_code_reverse(const String &pcode);
    
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

    void forward_reachable_inputs(Bitvector &, ErrorHandler *errh = 0) const;

    String compound_port_count_code() const;
    String compound_processing_code() const;
    String compound_flow_code(ErrorHandler *errh = 0) const;
    
  private:

    const RouterT *_router;
    ElementMap *_element_map;
    VariableEnvironment _scope;

    Vector<int> _input_pidx;
    Vector<int> _output_pidx;
    Vector<const ElementT *> _input_elt;
    Vector<const ElementT *> _output_elt;
    Vector<int> _input_processing;
    Vector<int> _output_processing;
    Vector<PortT> _connected_input;
    Vector<PortT> _connected_output;

    enum { classwarn_unknown = 1, classwarn_pcode = 2 };
    HashMap<ElementClassT *, int> _class_warnings;

    void create_pidx(ErrorHandler *);

    void initial_processing_for(int, const String &compound_pcode, ErrorHandler *);
    void initial_processing(const String &compound_pcode, ErrorHandler *);
    void processing_error(const ConnectionT &, int, ErrorHandler *);
    void check_processing(ErrorHandler *);
    void check_connections(ErrorHandler *);
    void check_nports(const ElementT *, const int *, const int *, ErrorHandler *);
    void resolve_agnostics();	// change remaining AGNOSTICs to PUSH

};


inline int
ProcessingT::input_pidx(const PortT &h) const
{
    assert(h.router() == _router);
    return input_pidx(h.eindex(), h.port);
}

inline int
ProcessingT::output_pidx(const PortT &h) const
{
    assert(h.router() == _router);
    return output_pidx(h.eindex(), h.port);
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
    const ElementT *e = _input_elt[pidx];
    return PortT(const_cast<ElementT *>(e), pidx - _input_pidx[e->eindex()]);
}

inline PortT
ProcessingT::output_port(int pidx) const
{
    const ElementT *e = _output_elt[pidx];
    return PortT(const_cast<ElementT *>(e), pidx - _output_pidx[e->eindex()]);
}

inline int
ProcessingT::input_processing(const PortT &h) const
{
    return _input_processing[input_pidx(h)] & 3;
}

inline int
ProcessingT::output_processing(const PortT &h) const
{
    return _output_processing[output_pidx(h)] & 3;
}

inline char
ProcessingT::decorated_input_processing_letter(const PortT &h) const
{
    return decorated_processing_letters[_input_processing[input_pidx(h)]];
}

inline char
ProcessingT::decorated_output_processing_letter(const PortT &h) const
{
    return decorated_processing_letters[_output_processing[output_pidx(h)]];
}

inline int
ProcessingT::input_processing(int i, int p) const
{
    return _input_processing[input_pidx(i, p)] & 3;
}

inline int
ProcessingT::output_processing(int i, int p) const
{
    return _output_processing[output_pidx(i, p)] & 3;
}

inline bool
ProcessingT::input_is_pull(int i, int p) const
{
    return _input_processing[input_pidx(i, p)] & VPULL;
}

inline bool
ProcessingT::output_is_push(int i, int p) const
{
    return _output_processing[output_pidx(i, p)] & VPUSH;
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
    return forward_flow(p.element->type()->flow_code(), p.port, p.element->noutputs(), bv, errh);
}

inline int
ProcessingT::backward_flow(const PortT &p, Bitvector *bv, ErrorHandler *errh)
{
    return backward_flow(p.element->type()->flow_code(), p.port, p.element->ninputs(), bv, errh);
}

inline const char *
ProcessingT::processing_code_output(const char *code, const char *end_code, const char *pos)
{
    if (!pos)
	pos = code;
    while (pos < end_code && *pos != '/')
	pos++;
    return (pos == end_code ? code : pos + 1);
}

inline void
ProcessingT::check(ErrorHandler *errh)
{
    create("", false, errh);
}

#endif
