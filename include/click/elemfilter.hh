// -*- c-basic-offset: 4; related-file-name: "../../lib/elemfilter.cc" -*-
#ifndef CLICK_ELEMFILTER_HH
#define CLICK_ELEMFILTER_HH
#include <click/element.hh>
CLICK_DECLS

class ElementFilter { public:

    /** @brief Construct an ElementFilter. */
    ElementFilter() {
    }

    /** @brief Destroy an ElementFilter. */
    virtual ~ElementFilter() {
    }

    /** @brief Determine whether an element or port matches this filter.
     * @param e element
     * @param isoutput true for output ports, false for input ports
     * @param port port number, or -1 to check the element as a whole
     *
     * This virtual function is the core of ElementFilter's functionality.
     * The function should return true iff the specified element or port
     * matches the filter.  @a isoutput and @a port define the interesting
     * port; if @a port < 0, the element should be checked as a whole.
     *
     * The default implementation returns false for any element.
     */
    virtual bool check_match(Element *e, bool isoutput, int port);

    /** @brief Remove all non-matching elements from @a es.
     * @param es array of elements
     *
     * Calls check_match(e, false, -1) for each element of @a es, removing
     * those elements that do not match (i.e., check_match() returns false).
     */
    void filter(Vector<Element *> &es);

};

class CastElementFilter : public ElementFilter { public:

    /** @brief Construct a CastElementFilter.
     * @param name cast name of matching elements
     */
    CastElementFilter(const String &name);

    /** @brief Determine whether an element matches this filter.
     * @param e element
     * @param isoutput ignored
     * @param port ignored
     * @return True iff @a e->cast(@a name) != NULL, where @a name is the
     *   cast name passed to the constructor.
     */
    bool check_match(Element *e, bool isoutput, int port);

  private:

    String _name;

};

CLICK_ENDDECLS
#endif
