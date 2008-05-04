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

    enum { end_to = ConnectionT::end_to, end_from = ConnectionT::end_from };
    
    int nelements() const {
	return _pidx[end_to].size() - 1;
    }
    int npidx(bool isoutput) const {
	return _pidx[isoutput].back();
    }
    ElementMap *element_map() const {
	return _element_map;
    }

    int pidx(int eindex, int port, bool isoutput) const {
	return _pidx[isoutput][eindex] + port;
    }
    int pidx(const PortT &port, bool isoutput) const {
	assert(port.router() == _router);
	return pidx(port.eindex(), port.port, isoutput);
    }
    int pidx(const ConnectionT &conn, bool isoutput) const {
	return pidx(conn.end(isoutput), isoutput);
    }

    PortT port(int pidx, bool isoutput) const {
	const ElementT *e = _elt[isoutput][pidx];
	return PortT(const_cast<ElementT *>(e), pidx - _pidx[isoutput][e->eindex()]);
    }

    int ninput_pidx() const {
	return npidx(end_to);
    }
    int input_pidx(int eindex, int port = 0) const {
	return pidx(eindex, port, end_to);
    }
    int input_pidx(const PortT &port) const {
	return pidx(port, end_to);
    }
    int input_pidx(const ConnectionT &conn) const {
	return pidx(conn, end_to);
    }
    PortT input_port(int pidx) const {
	return port(pidx, end_to);
    }
    
    int noutput_pidx() const {
	return npidx(end_from);
    }
    int output_pidx(int eindex, int port = 0) const {
	return pidx(eindex, port, end_from);
    }
    int output_pidx(const PortT &port) const {
	return pidx(port, end_from);
    }
    int output_pidx(const ConnectionT &conn) const {
	return pidx(conn, end_from);
    }
    PortT output_port(int pidx) const {
	return port(pidx, end_from);
    }

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

    /** @brief Set @a x to those ports reachable from @a port in @a code.
     * @param code flow code
     * @param port port number
     * @param isoutput whether @a port is an output port
     * @param[out] x output bitvector
     * @param size size of @a x (number of !@a isoutput ports)
     * @param errh error handler
     * @return 0 on success, -1 if @a code is invalid
     *
     * On return, @a x.size() == @a size, and @a x[@em i] is true if and only
     * if @a code indicates flow between port @a port and port @em i.  If @a
     * port < 0, then @a x is all false.
     *
     * For example, if @a code is @c "x/x", then @a x is all true.
     */
    static int code_flow(const String &code, int port, bool isoutput, Bitvector *x, int size, ErrorHandler *errh = 0);
    int port_flow(const PortT &port, bool isoutput, Bitvector *x, ErrorHandler *errh = 0) const {
	return code_flow(port.element->type()->flow_code(_element_map),
			 port.port, isoutput,
			 x, port.element->nports(!isoutput), errh);
    }
    
    static int forward_flow(const String &code, int input_port, Bitvector *x, int noutputs, ErrorHandler *errh = 0) {
	return code_flow(code, input_port, false, x, noutputs, errh);
    }
    int forward_flow(const PortT &port, Bitvector *x, ErrorHandler *errh = 0) {
	return port_flow(port, false, x, errh);
    }
    static int backward_flow(const String &code, int output_port, Bitvector *x, int ninputs, ErrorHandler *errh = 0) {
	return code_flow(code, output_port, true, x, ninputs, errh);
    }
    int backward_flow(const PortT &port, Bitvector *x, ErrorHandler *errh = 0) {
	return port_flow(port, true, x, errh);
    }

    /** @brief Set bits in @a sink that are connected to ports in @a source.
     * @param source source ports
     * @param source_isoutput whether @a source represents output ports
     * @param[out] sink sink ports
     */
    void follow_connections(const Bitvector &source, bool source_isoutput, Bitvector &sink) const;
    void follow_connections(const PortT &source, bool source_isoutput, Bitvector &sink) const;
    void follow_flow(const Bitvector &source, bool source_isoutput, Bitvector &sink, ErrorHandler *errh = 0) const;
    void follow_reachable(Bitvector &ports, bool isoutput, bool forward, ErrorHandler *errh = 0) const;

    String compound_port_count_code() const;
    String compound_processing_code() const;
    String compound_flow_code(ErrorHandler *errh = 0) const;
    
  private:

    const RouterT *_router;
    ElementMap *_element_map;
    VariableEnvironment _scope;

    Vector<int> _pidx[2];
    Vector<const ElementT *> _elt[2];
    Vector<int> _input_processing;
    Vector<int> _output_processing;
    Vector<PortT> _connected_input;
    Vector<PortT> _connected_output;

    enum { classwarn_unknown = 1, classwarn_pcode = 2 };
    HashTable<ElementClassT *, int> _class_warnings;

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
