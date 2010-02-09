// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPRW_PATTERN_HH
#define CLICK_IPRW_PATTERN_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/ipflowid.hh>
CLICK_DECLS
class IPRewriterFlow;
class IPRewriterEntry;
class IPRewriterInput;

class IPRewriterPattern { public:

    IPRewriterPattern(const IPAddress &saddr, int sport,
		      const IPAddress &daddr, int dport,
		      bool is_napt, bool sequential, bool same_first,
		      uint32_t variation);
    static bool parse(const Vector<String> &words, IPRewriterPattern **result,
		      Element *context, ErrorHandler *errh);
    static bool parse_ports(const Vector<String> &words, IPRewriterInput *input,
			    Element *model, ErrorHandler *errh);
    static bool parse_with_ports(const String &str, IPRewriterInput *input,
				 Element *context, ErrorHandler *errh);

    void use() {
	_refcount++;
    }
    void unuse() {
	if (--_refcount <= 0)
	    delete this;
    }

    operator bool() const {
	return _saddr || _sport || _daddr || _dport;
    }
    IPAddress daddr() const {
	return _daddr;
    }

    int rewrite_flowid(const IPFlowID &flowid, IPFlowID &rewritten_flowid,
		       const HashContainer<IPRewriterEntry> &reply_map);

    String unparse() const;

  private:

    IPAddress _saddr;
    int _sport;			// net byte order
    IPAddress _daddr;
    int _dport;			// net byte order

    uint32_t _variation_top;
    uint32_t _next_variation;

    bool _is_napt;
    bool _sequential;
    bool _same_first;

    int _refcount;

    IPRewriterPattern(const IPRewriterPattern&);
    IPRewriterPattern& operator=(const IPRewriterPattern&);

};

CLICK_ENDDECLS
#endif
