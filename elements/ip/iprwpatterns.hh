#ifndef CLICK_IPRWPATTERNS_HH
#define CLICK_IPRWPATTERNS_HH
#include "elements/ip/iprewriterbase.hh"
#include <click/hashtable.hh>
CLICK_DECLS

/*
 * =c
 * IPRewriterPatterns(NAME PATTERN, ...)
 * =s nat
 * specifies shared IPRewriter(n) patterns
 * =d
 *
 * This element stores information about shared patterns that IPRewriter and
 * related elements can use.  Each configuration argument is a name and a
 * pattern, 'NAME SADDR SPORT DADDR DPORT'.  The NAMEs for every argument in
 * every IPRewriterPatterns element in the configuration must be distinct.
 *
 * =a IPRewriter
 */

class IPRewriterPatterns : public Element { public:

    IPRewriterPatterns() CLICK_COLD;
    ~IPRewriterPatterns() CLICK_COLD;

    const char *class_name() const { return "IPRewriterPatterns"; }

    int configure_phase() const	{ return IPRewriterBase::CONFIGURE_PHASE_PATTERNS; }
    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

};

CLICK_ENDDECLS
#endif
