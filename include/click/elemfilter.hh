// -*- c-basic-offset: 4; related-file-name: "../../lib/elemfilter.cc" -*-
#ifndef CLICK_ELEMFILTER_HH
#define CLICK_ELEMFILTER_HH
#include <click/element.hh>
CLICK_DECLS

class ElementFilter { public:

    ElementFilter()			{ }
    virtual ~ElementFilter()		{ }

    inline bool match_input(Element *, int port);
    inline bool match_output(Element *, int port);
    inline bool match(Element *);

    enum PortType { NONE, INPUT, OUTPUT };
    virtual bool check_match(Element *e, int port, PortType);

    void filter(Vector<Element *> &);

};

class CastElementFilter : public ElementFilter { public:

    CastElementFilter(const String &);
    bool check_match(Element *, int, PortType);

  private:

    String _what;

};


inline bool
ElementFilter::match_input(Element *e, int port)
{
    return check_match(e, port, INPUT);
}

inline bool
ElementFilter::match_output(Element *e, int port)
{
    return check_match(e, port, OUTPUT);
}

CLICK_ENDDECLS
#endif
