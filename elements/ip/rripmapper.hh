#ifndef CLICK_RRIPMAPPER_HH
#define CLICK_RRIPMAPPER_HH
#include "elements/ip/iprewriterbase.hh"
CLICK_DECLS

/*
 * =c
 * RoundRobinIPMapper(PATTERN1, ..., PATTERNn)
 * =s nat
 * round-robin mapper for IPRewriter(n)
 * =d
 *
 * Works in tandem with IPRewriter to provide round-robin rewriting. This is
 * useful, for example, in load-balancing applications. Implements the
 * IPMapper interface.
 *
 * Responds to mapping requests from an IPRewriter by trying the PATTERNs in
 * round-robin order and returning the first successfully created mapping.
 *
 * =a IPRewriter, TCPRewriter, IPRewriterPatterns */

class RoundRobinIPMapper : public Element, public IPMapper { public:

    RoundRobinIPMapper();
    ~RoundRobinIPMapper();

    const char *class_name() const	{ return "RoundRobinIPMapper"; }
    void *cast(const char *);

    int configure_phase() const		{ return IPRewriterBase::CONFIGURE_PHASE_MAPPER;}
    int configure(Vector<String> &conf, ErrorHandler *errh);
    void cleanup(CleanupStage);

    void notify_rewriter(IPRewriterBase *user, IPRewriterInput *input,
			 ErrorHandler *errh);
    int rewrite_flowid(IPRewriterInput *input,
		       const IPFlowID &flowid, IPFlowID &rewritten_flowid,
		       Packet *p, int mapid);

 private:

    Vector<IPRewriterInput> _is;
    int _last_pattern;

};

CLICK_ENDDECLS
#endif
