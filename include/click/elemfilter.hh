// -*- c-basic-offset: 4; related-file-name: "../../lib/elemfilter.cc" -*-
#ifndef CLICK_ELEMFILTER_HH
#define CLICK_ELEMFILTER_HH
#include <click/element.hh>
CLICK_DECLS

class ElementFilter { public:

    ElementFilter()			: _match_count(0) { }
    virtual ~ElementFilter()		{ }

    bool match(Element *e, int port);
    virtual bool check_match(Element *e, int port);

    int match_count() const		{ return _match_count; }

    void filter(Vector<Element *> &);

  private:

    int _match_count;

};

class CastElementFilter : public ElementFilter { public:

    CastElementFilter(const String &);
    bool check_match(Element *, int);

  private:

    String _what;

};


inline bool
ElementFilter::match(Element *e, int port)
{
    bool m = check_match(e, port);
    if (m)
	_match_count++;
    return m;
}

inline bool
ElementFilter::check_match(Element *, int)
{
    return false;
}

CLICK_ENDDECLS
#endif
