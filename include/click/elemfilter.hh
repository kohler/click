// -*- c-basic-offset: 4; related-file-name: "../../lib/elemfilter.cc" -*-
#ifndef CLICK_ELEMFILTER_HH
#define CLICK_ELEMFILTER_HH
#include <click/element.hh>

class ElementFilter { public:

    ElementFilter()				{ }
    virtual ~ElementFilter()			{ }

    virtual bool match(Element *e, int port);

    void filter(Vector<Element *> &);

};

class CastElementFilter : public ElementFilter { public:

    CastElementFilter(const String &);
    bool match(Element *, int);

  private:

    String _what;

};

class InputProcessingElementFilter : public ElementFilter { public:

    InputProcessingElementFilter(bool push);
    bool match(Element *, int);

  private:

    bool _push;

};

class OutputProcessingElementFilter : public ElementFilter { public:

    OutputProcessingElementFilter(bool push);
    bool match(Element *, int);

  private:

    bool _push;

};

class DisjunctionElementFilter : public ElementFilter { public:

    DisjunctionElementFilter()		{ }
    void add(ElementFilter *f)		{ _filters.push_back(f); }
    bool match(Element *, int);

  private:

    Vector<ElementFilter *> _filters;

};


inline bool
ElementFilter::match(Element *, int)
{
    return false;
}

#endif
