// -*- c-basic-offset: 4 -*-
#ifndef CLICK_PROCESSINGT_HH
#define CLICK_PROCESSINGT_HH
#include "routert.hh"
class ElementMap;
class Bitvector;

class ProcessingT { public:

    enum {
	end_to = ConnectionT::end_to,
	end_from = ConnectionT::end_from
    };

    enum ProcessingCode {
	ppush = 1, ppull = 2, pagnostic = 4, perror = 8
    };
    static const char processing_letters[];
    static const char decorated_processing_letters[];

    ProcessingT(bool resolve_agnostics, RouterT *router, ElementMap *emap,
		ErrorHandler *errh = 0);
    ProcessingT(RouterT *router, ElementMap *emap, ErrorHandler *errh = 0);
    ProcessingT(const ProcessingT &processing, ElementT *element,
		ErrorHandler *errh = 0);
    void check_types(ErrorHandler *errh = 0);

    RouterT *router() const {
	return _router;
    }
    const VariableEnvironment &scope() const {
	return _scope;
    }
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

    const String &flow_code(ElementT *e) const {
	if (_flow_overrides.size())
	    if (const String &fc = _flow_overrides.get(e->name()))
		return fc;
	return e->flow_code(_element_map);
    }

    int input_processing(const PortT &port) const;
    int output_processing(const PortT &port) const;
    char decorated_input_processing_letter(const PortT &port) const;
    char decorated_output_processing_letter(const PortT &port) const;
    int input_processing(int eindex, int port) const;
    int output_processing(int eindex, int port) const;

    bool input_is_pull(int eindex, int port) const;
    bool output_is_push(int eindex, int port) const;

    bool processing_error(int pidx, bool isoutput) const {
	return _processing[isoutput][pidx] & perror;
    }

    bool same_processing(int a_eindex, int b_eindex) const;

    String processing_code(const ElementT *element) const;
    String decorated_processing_code(const ElementT *element) const;

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
	return code_flow(flow_code(port.element), port.port, isoutput,
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
     * @param[out] sink sink ports (input if @a source_isoutput, and vice versa)
     */
    void follow_connections(const Bitvector &source, bool source_isoutput, Bitvector &sink) const;
    void follow_connections(const PortT &source, bool source_isoutput, Bitvector &sink) const;
    void follow_flow(const Bitvector &source, bool source_isoutput, Bitvector &sink, ErrorHandler *errh = 0) const;
    void follow_reachable(Bitvector &ports, bool isoutput, bool forward, ErrorHandler *errh = 0, ErrorHandler *debug_errh = 0) const;

    String compound_port_count_code() const;
    String compound_processing_code() const;
    String compound_flow_code(ErrorHandler *errh = 0) const;

  private:

    RouterT *_router;
    String _router_name;
    ElementMap *_element_map;
    VariableEnvironment _scope;
    HashTable<String, String> _flow_overrides;

    Vector<int> _pidx[2];
    Vector<const ElementT *> _elt[2];
    Vector<int> _processing[2];
    bool _pidx_created;

    enum { classwarn_unknown = 1, classwarn_pcode = 2 };
    HashTable<ElementClassT *, int> _class_warnings;

    void parse_flow_info(ElementT *e, ErrorHandler *errh);
    void create_pidx(ErrorHandler *errh);
    void create(const String &compound_pcode, bool resolve_agnostics, ErrorHandler *errh);

    void initial_processing_for(int, const String &compound_pcode, ErrorHandler *);
    void initial_processing(const String &compound_pcode, ErrorHandler *);
    void processing_error(const ConnectionT &, int, ErrorHandler *);
    void check_processing(Vector<ConnectionT> &conn, Bitvector &invalid_conn, ErrorHandler *errh);
    void check_connections(Vector<ConnectionT> &conn, Bitvector &invalid_conn, ErrorHandler *errh);
    void check_nports(Vector<ConnectionT> &conn, const ElementT *, const int *, const int *, ErrorHandler *);
    void resolve_agnostics();	// change remaining AGNOSTICs to PUSH
    void debug_print_pidxes(const Bitvector &ports, bool isoutput, const String &prefix, ErrorHandler *debug_errh) const;

};


inline int
ProcessingT::input_processing(const PortT &h) const
{
    return _processing[end_to][input_pidx(h)] & 3;
}

inline int
ProcessingT::output_processing(const PortT &h) const
{
    return _processing[end_from][output_pidx(h)] & 3;
}

inline char
ProcessingT::decorated_input_processing_letter(const PortT &h) const
{
    return decorated_processing_letters[_processing[end_to][input_pidx(h)] & 7];
}

inline char
ProcessingT::decorated_output_processing_letter(const PortT &h) const
{
    return decorated_processing_letters[_processing[end_from][output_pidx(h)] & 7];
}

inline int
ProcessingT::input_processing(int i, int p) const
{
    return _processing[end_to][input_pidx(i, p)] & 3;
}

inline int
ProcessingT::output_processing(int i, int p) const
{
    return _processing[end_from][output_pidx(i, p)] & 3;
}

inline bool
ProcessingT::input_is_pull(int i, int p) const
{
    return _processing[end_to][input_pidx(i, p)] & ppull;
}

inline bool
ProcessingT::output_is_push(int i, int p) const
{
    return _processing[end_from][output_pidx(i, p)] & ppush;
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

#endif
