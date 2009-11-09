// -*- c-basic-offset: 4; related-file-name: "../../lib/routervisitor.cc" -*-
#ifndef CLICK_ROUTERVISITOR_HH
#define CLICK_ROUTERVISITOR_HH
#include <click/element.hh>
#include <click/bitvector.hh>
CLICK_DECLS

/** @class RouterVisitor
 * @brief Base class for router configuration visitors.
 *
 * RouterVisitor objects are used to traverse the router configuration graph.
 * They are usually passed to the Router::visit_downstream() and
 * Router::visit_upstream() functions.
 */
class RouterVisitor { public:

    /** @brief Construct an RouterVisitor. */
    RouterVisitor() {
    }

    /** @brief Destroy an RouterVisitor. */
    virtual ~RouterVisitor() {
    }

    /** @brief Visit an element.
     * @param e element being visited
     * @param isoutput true for output ports, false for input ports
     * @param port port number being visited
     * @param from_e element by which @a e was reached
     * @param from_port port by which @a e/@a port was reached
     * @param distance connection distance in breadth-first search
     * @return true to continue traversing, false to stop
     *
     * A Router configuration traversal calls this function once on each
     * reached port.  Configuration traversal is breadth-first; @a distance is
     * the minimum connection distance from the traversal's start point to @a
     * e/@a port.  A traversal will call a port's visit() function at most
     * once.  @a from_e and @a from_port specify one port that is connected to
     * @a e and @a port (there may be more than one).  If @a isoutput is true,
     * then @a e/@a port is an output port and @a from_e/@a from_port is an
     * input port; if @a isoutput is false, the opposite holds.
     *
     * The return value specifies whether traversal should continue through
     * the @a e element.  If it is true, then the traversal will continue
     * based on @a e's packet flow specifier, via ports that connect to @a
     * e/@a port (see Element::flow_code() for more).
     *
     * The default implementation always returns true.
     */
    virtual bool visit(Element *e, bool isoutput, int port,
		       Element *from_e, int from_port, int distance);

};

/** @class ElementTracker
 * @brief Base class for router configuration visitors that collect elements.
 *
 * ElementTracker is a type of RouterVisitor used to traverse the router
 * configuration graph and collect matching elements.  A subclass's @link
 * RouterVisitor::visit visit() @endlink function will call insert() to store
 * elements that match.  ElementTracker objects are usually passed to the
 * Router::visit_downstream() and Router::visit_upstream() functions.
 */
class ElementTracker : public RouterVisitor { public:

    /** @brief Construct an ElementTracker.
     * @param router the router to be traversed */
    ElementTracker(Router *router);

    /** @brief Return the elements that matched. */
    const Vector<Element *> &elements() const {
	return _elements;
    }
    /** @brief Return the number of matching elements. */
    int size() const {
	return _elements.size();
    }
    /** @brief Return the @a i'th matching element.
     *
     * Elements are ordered by their time of first insertion. */
    Element *operator[](int i) const {
	return _elements[i];
    }

    /** @brief Iterator type. */
    typedef Vector<Element *>::const_iterator const_iterator;
    /** @brief Return an iterator for the first matching element. */
    const_iterator begin() const {
	return _elements.begin();
    }
    /** @brief Return an iterator for the end of the container. */
    const_iterator end() const {
	return _elements.end();
    }

    /** @brief Return true iff element @a e is a matching element. */
    bool contains(const Element *e) const {
	return _reached[e->eindex()];
    }

    /** @brief Add element @a e to the set of matching elements. */
    void insert(Element *e) {
	if (!_reached[e->eindex()]) {
	    _reached[e->eindex()] = true;
	    _elements.push_back(e);
	}
    }

    /** @brief Clear the set of matching elements. */
    void clear() {
	_reached.clear();
	_elements.clear();
    }

  private:

    Bitvector _reached;
    Vector<Element *> _elements;

};

/** @class ElementCastTracker
 * @brief Router configuration visitor that collects elements that match a
 * certain cast.
 *
 * When passed to Router::visit_upstream() or Router::visit_downstream(),
 * ElementCastTracker collects the closest elements that successfully cast to
 * @a name.  For instance, this code will find the closest Storage elements
 * upstream of [0]@a e:
 * @code
 * ElementCastTracker tracker(e->router(), "Storage");
 * e->router()->visit_upstream(e, 0, &tracker);
 * tracker.elements();  // a Vector<Element *> containing the Storage elements
 * @endcode
 *
 * Graph traversal stops at each matching element, so in the above example, a
 * Storage element that is "behind" another Storage element won't be returned.
 */
class ElementCastTracker : public ElementTracker { public:

    /** @brief Construct an ElementCastTracker.
     * @param router the router to be traversed
     * @param name the cast of interest */
    ElementCastTracker(Router *router, const String &name)
	: ElementTracker(router), _name(name) {
    }

    bool visit(Element *e, bool isoutput, int port,
	       Element *from_e, int from_port, int distance);

  private:

    String _name;

};

/** @class ElementNeighborhoodTracker
 * @brief Router configuration visitor that collects close-by elements.
 *
 * When passed to Router::visit_upstream() or Router::visit_downstream(),
 * ElementNeighborhoodTracker collects the elements that are within a certain
 * number of connections of the source element.  For instance, this code will
 * find all the elements connected to [0]@a e:
 * @code
 * ElementNeighborhoodTracker tracker(e->router());
 * e->router()->visit_upstream(e, 0, &tracker);
 * tracker.elements();  // Vector<Element *> containing neighboring elements
 * @endcode
 *
 * Supply the constructor's @a diameter argument to find a larger neighborhood
 * than just the directly-connected elements.
 */
class ElementNeighborhoodTracker : public ElementTracker { public:

    /** @brief Construct an ElementNeighborhoodTracker.
     * @param router the router to be traversed
     * @param diameter neighborhood diameter (maximum number of connections to
     * traverse) */
    ElementNeighborhoodTracker(Router *router, int diameter = 1)
	: ElementTracker(router), _diameter(diameter) {
    }

    bool visit(Element *e, bool isoutput, int port,
	       Element *from_e, int from_port, int distance);

  private:

    int _diameter;

};

CLICK_ENDDECLS
#endif
