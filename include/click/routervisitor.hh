// -*- c-basic-offset: 4; related-file-name: "../../lib/routervisitor.cc" -*-
#ifndef CLICK_ROUTERVISITOR_HH
#define CLICK_ROUTERVISITOR_HH
#include <click/element.hh>
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

CLICK_ENDDECLS
#endif
