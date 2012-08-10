// -*- c-basic-offset: 4; related-file-name: "../include/click/element.hh" -*-
/*
 * element.{cc,hh} -- the Element base class
 * Eddie Kohler
 * statistics: Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2004-2011 Regents of the University of California
 * Copyright (c) 2010 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software")
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#define CLICK_ELEMENT_DEPRECATED /* Avoid warnings in this file */

#include <click/glue.hh>
#include <click/element.hh>
#include <click/bitvector.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/master.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#if CLICK_DEBUG_SCHEDULING
# include <click/notifier.hh>
#endif
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <asm/types.h>
# include <asm/uaccess.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#elif CLICK_BSDMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/types.h>
# include <sys/limits.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

const char Element::PORTS_0_0[] = "0";
const char Element::PORTS_0_1[] = "0/1";
const char Element::PORTS_1_0[] = "1/0";
const char Element::PORTS_1_1[] = "1";
const char Element::PORTS_1_1X2[] = "1/1-2";

const char Element::AGNOSTIC[] = "a";
const char Element::PUSH[] = "h";
const char Element::PULL[] = "l";
const char Element::PUSH_TO_PULL[] = "h/l";
const char Element::PULL_TO_PUSH[] = "l/h";
const char Element::PROCESSING_A_AH[] = "a/ah";

const char Element::COMPLETE_FLOW[] = "x/x";

int Element::nelements_allocated = 0;

/** @mainpage Click
 *  @section  Introduction
 *
 *  The Click modular router is a system for developing network packet
 *  processors.  This documentation describes Click's internal programming
 *  interfaces, which you will use if you want to write new packet processing
 *  modules (which we call <em>elements</em>).
 *
 *  Most documented Click classes can be found under the "Classes" tab.  Get
 *  started by looking at the Element class, or the args.hh header file
 *  for configuration string parsing.
 *
 *  <a href='http://www.read.cs.ucla.edu/click/'>Main Click page</a>
 */

/** @class Element
  @brief Base class for Click elements.

  Click programmers spend most of their time writing elements, which are
  subclasses of class Element.  Element provides functionality of its own,
  particularly the input() and output() methods and associated Element::Port
  objects.  More important, however, is the set of functions that derived
  classes override to define element behavior.  Good Click programmers
  understand how the Click system uses these functions to manipulate and
  initialize elements.  These functions fall into several categories:

  <dl>
  <dt>Behavior specifications</dt>
  <dd>These functions return static constants that describe element
  properties, such as class names, valid numbers of ports, and port processing
  types.  Their values are automatically extracted from element source code
  for use by tools, so your source code should follow a specific syntax.
  Examples: class_name(), port_count(), processing(), flow_code(), flags().</dd>
  <dt>Configuration, initialization, and cleanup</dt>
  <dd>Configuration and initialization functions are called to set up an
  element as a router is initialized (or when the element itself is
  reconfigured).  Most of the functions are passed an ErrorHandler argument,
  to which they should report any errors.  By returning negative values, they
  can prevent the router from initializing.  Other functions clean up elements
  when a router is removed and reconfigure an element as the router runs.
  Examples: cast(), configure(), configure_phase(), add_handlers(),
  initialize(), take_state(), cleanup(), can_live_reconfigure(),
  live_reconfigure().</dd>
  <dt>Packet and event processing</dt>
  <dd>These functions are called as the router runs to process packets and
  other events.  Examples: push(), pull(), simple_action(), run_task(),
  run_timer(), selected().</dd>
  </dl>

  <h3>Examples</h3>

  Here is the simplest possible element definition.

@code
class MyNothingElement : public Element { public:
    MyNothingElement() { }
    ~MyNothingElement() { }
    const char *class_name() const { return "MyNothingElement"; }
};
@endcode

  This element has no ports and accepts no configuration arguments; it does
  nothing.  The required class_name() function informs Click's infrastructure
  of the element's class name.

  Although this element is code-complete, Click's build process requires a bit
  more boilerplate, like so:

@code
// ================== elements/local/mynothingelement.hh ==================
#ifndef CLICK_MYNOTHINGELEMENT_HH
#define CLICK_MYNOTHINGELEMENT_HH
#include <click/element.hh>
CLICK_DECLS
class MyNothingElement : public Element { public:
    MyNothingElement() { }
    ~MyNothingElement() { }
    const char *class_name() const { return "MyNothingElement"; }
};
CLICK_ENDDECLS
#endif

// ================== elements/local/mynothingelement.cc ==================
#include <click/config.h>
#include "mynothingelement.hh"
CLICK_DECLS
// Non-inline element code would go here.
CLICK_ENDDECLS
EXPORT_ELEMENT(MyNothingElement)
@endcode

  Some things to notice:

  - The element class must be defined in a header file and a source file.
  - The header file is protected from multiple inclusion.  A common error is
    to copy and paste another element's header file, but forget to change the
    header protection symbol (here, CLICK_MYNOTHINGELEMENT_HH).
  - All Click declarations are enclosed within a macro pair,
    <tt>CLICK_DECLS</tt> and <tt>CLICK_ENDDECLS</tt>.  These are required for
    the NS and FreeBSD kernel drivers.  Note that <tt>\#include</tt> statements
    go @em outside the <tt>CLICK_DECLS/CLICK_ENDDECLS</tt> pair.
  - The element's C++ class is defined in the header file.
  - The first thing the source file does is <tt>\#include
    <click/config.h></tt>.  <strong>This is mandatory.</strong>
  - The source file must contain a line like
    <tt>EXPORT_ELEMENT(NameOfC++ClassForElement)</tt>.  Click's compilation
    process will ignore your element unless there's a line like this.

  This slightly more complex example illustrates some more of Click's
  element infrastructure.

@code
class MyNullElement : public Element { public:
    MyNullElement() { }
    ~MyNullElement() { }
    const char *class_name() const { return "MyNullElement"; }
    const char *port_count() const { return PORTS_1_1; }
    const char *processing() const { return PUSH; }
    void push(int port, Packet *p) {
        output(0).push(p);
    }
};
@endcode

  This element processes packets in push mode, much like the standard @e
  PushNull element.

  <ul>
  <li>The port_count() method tells Click that this element has one input and
    one output.  See port_count() for more.</li>
  <li>The processing() method tells Click that this element's ports are in push
    mode.  See processing() for more.</li>
  <li>The element doesn't define a configure() or initialize() method, so the
    defaults are used:  the element takes no configuration arguments, and
    always initializes successfully.</li>
  <li>The element has just one function called after router initialization,
    namely push().  This function is called when another element pushes a
    packet to this element.  The implementation here simply pushes the packet
    to the element's first output port.</li>
  <li><strong>Invariants:</strong> Click's initialization process checks for
    many kinds of errors, allowing the push() method to assume several
    invariants.  In particular, port_count() and processing() specify that
    this element has one push input and one push output.  Click therefore
    ensures that the element's first input is used in a connection at least
    once; that its first output is used in a connection
    <em>exactly</em> once; that its other inputs and outputs are not
    used at all; and that all connections involving the element's ports are
    push.

    As a result, the push() method can assume that <tt>port == 0</tt>, that
    <tt>output(0)</tt> exists, and that <tt>output(0).push(p)</tt> is valid.

    Elements must not push null packet pointers, either, so the push() method
    can assume that <tt>p != 0</tt>.

    There is no harm in verifying these invariants with assertions, since
    bogus element code can violate them (by passing a bad value for
    <tt>port</tt> or <tt>p</tt>, for example), but such errors are rare in
    practice.  Our elements mostly assume that the invariants
    hold.</li>
  </ul>

  <h3>Packet Accounting</h3>

  Element run-time methods, such as push(), pull(), run_task(), and
  run_timer(), must always obey the following rules:

  - Each Packet pointer is used by at most one element at a time.
  - An element that obtains a Packet pointer must eventually free it or pass
    it to another element.  This avoids memory leaks.

  Beginning Click programmers often violate these rules.  Here are some
  examples to make them concrete.

This incorrect version of Click's @e Tee element attempts to duplicate a
packet and send the duplicates to two different outputs.

@code
void BadTee::push(int port, Packet *p) {
    output(0).push(p);
    output(1).push(p);
}
@endcode

A single packet pointer @c p has been pushed to two different outputs. This is
always illegal; the rest of the configuration may have modified or even freed
the packet before returning control to @e BadTee, so
<tt>output(1).push(p)</tt> would probably touch freed memory!  This situation
requires the Packet::clone() method, which makes a copy of a packet:

@code
void BetterTee::push(int port, Packet *p) {
    output(0).push(p->clone());
    output(1).push(p);
}
@endcode

However, @e BetterTee would fail if the router ran out of memory for packet
clones.  @c p->clone() would return null, and passing a null pointer to
another element's push() method isn't allowed.  Here's how to fix this:

@code
void BestTee::push(int port, Packet *p) {
    if (Packet *clone = p->clone())
        output(0).push(clone);
    output(1).push(p);
}
@endcode

Here's an example of a push() method with an obvious leak:

@code
void LeakyCounter::push(int port, Packet *p) {
    _counter++;
}
@endcode

The method doesn't do anything with @c p, so its memory will never be
reclaimed.  Instead, it should either free the packet or pass it on to another
element:

@code
void BetterCounter1::push(int port, Packet *p) {
    _counter++;
    p->kill();            // free packet
}

void BetterCounter2::push(int port, Packet *p) {
    _counter++;
    output(0).push(p);    // push packet on
}
@endcode

Leaks involving error conditions are more common in practice.  For instance,
this push() method counts IP packets.  The programmer has defensively checked
whether or not the input packet's network header pointer is set.

@code
void LeakyIPCounter::push(int port, Packet *p) {
    if (!p->has_network_header())
        return;
    _counter++;
    output(0).push(p);
}
@endcode

Close, but no cigar: if the input packet has no network header pointer, the
packet will leak.  Here are some better versions.

@code
void BetterIPCounter1::push(int port, Packet *p) {
    // In this version, non-IP packets are dropped.  This is closest to LeakyIPCounter's intended functionality.
    if (!p->has_network_header()) {
        p->kill();
	return;
    }
    _counter++;
    output(0).push(p);
}

void BetterIPCounter2::push(int port, Packet *p) {
    // This programmer thinks non-IP packets are serious errors and should cause a crash.
    assert(p->has_network_header());
    _counter++;
    output(0).push(p);
}

void BetterIPCounter3::push(int port, Packet *p) {
    // This programmer passes non-IP packets through without counting them.
    if (p->has_network_header())
        _counter++;
    output(0).push(p);
}
@endcode

  <h3>Initialization Phases</h3>

  The Click infrastructure calls element initialization functions in a
  specific order during router initialization.  Errors at any stage prevent
  later stages from running.

  -# Collects element properties, specifically configure_phase(),
     port_count(), flow_code(), processing(), and can_live_reconfigure().
  -# Calculates how many of each element's input and output ports are used in
     the configuration.  There is an error if port_count() doesn't allow the
     result.
  -# Calculates each port's push or pull status.  This depends on processing()
     values, and for agnostic ports, a constraint satisfaction algorithm that
     uses flow_code().
  -# Checks that every connection is between two push ports or two pull ports;
     that there are no agnostic port conflicts (each port is used as push or
     pull, never both); that no port goes unused; and that push output ports
     and pull input ports are connected exactly once.  Violations cause an
     error.
  -# Sorts the elements by configure_phase() to construct a configuration
     order.
  -# Calls each element's configure() method in order, passing its
     configuration arguments and an ErrorHandler.  All configure() functions
     are called, even if a prior configure() function returns an error.
  -# Calls every element's add_handlers() method.
  -# Calls every element's initialize() method in configuration order.
     Initialization is aborted as soon as any method returns an error
     (i.e., some initialize() methods might not be called).
  -# At this point, the router will definitely be installed.  If the router
     was installed with a hotswap option, then Click searches the old and new
     router for potentially compatible pairs using hotswap_element(), and
     calls take_state() for each pair.  Any errors are ignored.
  -# Installs the router.

  Router cleanup takes place as follows.  Click:

  -# Removes all element handlers.
  -# Calls each element's cleanup() function in reverse configuration order.
     The argument to cleanup() indicates where the initialization process
     completed for that element.  See cleanup() for the specific constant
     names.
  -# Deletes each element.  This step might be delayed relative to cleanup()
     to allow the programmer to examine an erroneous router's state.
*/

/** @class Element::Port
 * @brief An Element's ports.
 *
 * Each of an element's ports has an associated Port object, accessible via
 * the Element::port(), Element::input(), and Element::output() functions.
 * Each @em active port knows, and can transfer a packet with, the single
 * complementary port to which it is connected.  Thus, each push output knows
 * its connected input, and can push a packet there; and each pull input can
 * pull a packet from its connected output.  Inactive ports -- push inputs and
 * pull outputs -- can be connected multiple times.  Their corresponding Port
 * objects have very little functionality.
 *
 * Element authors generally encounter Port objects only in two stylized
 * formulations.  First, to push a packet @c p out on push output port @c i:
 *
 * @code
 * output(i).push(p);
 * @endcode
 *
 * And second, to pull a packet @c p from pull input port @c i:
 *
 * @code
 * Packet *p = input(i).pull();
 * @endcode
 *
 * @sa Element::checked_output_push() */

/** @brief Construct an Element. */
Element::Element()
    : _router(0), _eindex(-1)
{
    nelements_allocated++;
    _ports[0] = _ports[1] = &_inline_ports[0];
    _nports[0] = _nports[1] = 0;

#if CLICK_STATS >= 2
    reset_cycles();
#endif
}

Element::~Element()
{
    nelements_allocated--;
    if (_ports[0] < _inline_ports || _ports[0] > _inline_ports + INLINE_PORTS)
	delete[] _ports[0];
    if (_ports[1] < _inline_ports || _ports[1] > _inline_ports + INLINE_PORTS)
	delete[] _ports[1];
}

// CHARACTERISTICS

/** @fn Element::class_name() const
 * @brief Return the element's class name.
 *
 * Each element class must override this function to return its class name.
 *
 * Click tools extract class names from the source.  For Click to find a class
 * name, the function definition must appear inline, on a single line, inside
 * the element class's declaration, and must return a C string constant.  It
 * should also have public accessibility.  Here's an acceptable class_name()
 * definition:
 *
 * @code
 * const char *class_name() const     { return "ARPQuerier"; }
 * @endcode
 */

/** @brief Attempt to cast the element to a named type.
 * @param name name of the type being cast to
 *
 * Click calls this function to see whether this element has a given type,
 * identified by @a name.  Thus, cast() is Click's version of the C++ @c
 * dynamic_cast operator.  (@c dynamic_cast itself is not available in the
 * Linux kernel, so we rolled our own.)  The function should return a pointer
 * to the named object, or a null pointer if this element doesn't have that
 * type.  @a name can name an element class or another type of interface, such
 * as @c "Storage" or Notifier::EMPTY_NOTIFIER.
 *
 * The default implementation returns this element if @a name equals
 * class_name(), and null otherwise.
 *
 * You should override cast() if your element inherits from another element
 * (and you want to expose that inheritance to Click); the resulting cast()
 * method will check both class names.  For example, if element @a Derived
 * inherited from element @a Base, Derived::cast() might be defined like this:
 *
 * @code
 * void *Derived::cast(const char *name) {
 *     if (strcmp(name, "Derived") == 0)
 *         return (Derived *) this;
 *     else if (strcmp(name, "Base") == 0)
 *         return (Base *) this;
 *     else
 *         return Base::cast(name);
 * }
 * @endcode
 *
 * The recursive call to Base::cast() is useful in case @e Base itself
 * overrides cast().  The explicit check for the name @c "Base" is necessary
 * in case @e Base did @e not override cast(): the default cast()
 * implementation compares against class_name(), which in this case is @c
 * "Derived".  Always explicitly cast @c this to the correct type before
 * returning it.
 *
 * You should also override cast() if your element provides another interface,
 * such as Storage or a Notifier.
 *
 * @sa port_cast
 */
void *
Element::cast(const char *name)
{
    const char *my_name = class_name();
    if (my_name && name && strcmp(my_name, name) == 0)
	return this;
    else
	return 0;
}

/** @brief Attempt to cast an element's port to a named type.
 * @param isoutput false for input ports, true for output ports
 * @param port port number
 * @param name name of the type being cast to
 *
 * Click calls this function to see whether a port corresponds to an object of
 * the type called @a name.  The function should return a pointer to the named
 * object, or a null pointer if this port doesn't have that type.  @a name can
 * name an element class or another type of interface, such as @c "Storage" or
 * Notifier::EMPTY_NOTIFIER.
 *
 * The default implementation returns the result of cast(), ignoring the @a
 * isoutput and @a port arguments.
 *
 * The cast() method suffices for most purposes, but some Click functionality,
 * such as Notifiers, can use the additional precision of port_cast().
 *
 * @sa cast
 */
void *
Element::port_cast(bool isoutput, int port, const char *name)
{
    (void) isoutput, (void) port;
    return cast(name);
}

/** @brief Return the element's name.
 *
 * This is the name used to declare the element in the router configuration,
 * with all compound elements expanded. */
String
Element::name() const
{
    String s;
    if (Router *r = router())
	s = r->ename(_eindex);
    return (s ? s : String::make_stable("<unknown>", 9));
}

/** @cond never */
/** @brief Return the element's name (deprecated).
 *
 * @deprecated This function is deprecated; use name() instead. */
String
Element::id() const
{
    return name();
}

/** @brief Return a string describing where the element was declared.
 *
 * The string generally has the form
 * &quot;<em>filename</em>:<em>linenumber</em>&quot;. */
String
Element::landmark() const
{
    String s;
    if (Router *r = router())
	s = r->elandmark(_eindex);
    return (s ? s : String::make_stable("<unknown>", 9));
}
/** @endcond never */

/** @brief Return a string giving the element's name and class name.
 *
 * The result has the form &quot;@e name :: @e class_name&quot;.  Element
 * classes can override this function to supply additional important
 * information, if desired; for example, @e FromDump returns a string &quot;@e
 * name :: @e class_name(@e filename)&quot;.
 */
String
Element::declaration() const
{
    return name() + " :: " + class_name();
}

// INPUTS AND OUTPUTS

int
Element::set_nports(int new_ninputs, int new_noutputs)
{
    // exit on bad counts, or if already initialized
    if (new_ninputs < 0 || new_noutputs < 0)
	return -EINVAL;
    if (_router && _router->_have_connections) {
	if (_router->_state >= Router::ROUTER_PREINITIALIZE)
	    return -EBUSY;
	_router->_have_connections = false;
    }

    // decide if inputs & outputs were inlined
    bool old_in_inline =
	(_ports[0] >= _inline_ports && _ports[0] <= _inline_ports + INLINE_PORTS);
    bool old_out_inline =
	(_ports[1] >= _inline_ports && _ports[1] <= _inline_ports + INLINE_PORTS);
    bool prefer_pull = (processing() == PULL);

    // decide if inputs & outputs should be inlined
    bool new_in_inline =
	(new_ninputs == 0
	 || new_ninputs + new_noutputs <= INLINE_PORTS
	 || (new_ninputs <= INLINE_PORTS && new_noutputs > INLINE_PORTS)
	 || (new_ninputs <= INLINE_PORTS && prefer_pull));
    bool new_out_inline =
	(new_noutputs == 0
	 || new_ninputs + new_noutputs <= INLINE_PORTS
	 || (new_noutputs <= INLINE_PORTS && !new_in_inline));

    // create new port arrays
    Port *new_inputs;
    if (new_in_inline)
	new_inputs = _inline_ports + (!new_out_inline || prefer_pull ? 0 : new_noutputs);
    else if (!(new_inputs = new Port[new_ninputs]))
	return -ENOMEM;

    Port *new_outputs;
    if (new_out_inline)
	new_outputs = _inline_ports + (!new_in_inline || !prefer_pull ? 0 : new_ninputs);
    else if (!(new_outputs = new Port[new_noutputs])) {
	if (!new_in_inline)
	    delete[] new_inputs;
	return -ENOMEM;
    }

    // install information
    if (!old_in_inline)
	delete[] _ports[0];
    if (!old_out_inline)
	delete[] _ports[1];
    _ports[0] = new_inputs;
    _ports[1] = new_outputs;
    _nports[0] = new_ninputs;
    _nports[1] = new_noutputs;
    return 0;
}

/** @brief Return the element's port count specifier.
 *
 * An element class overrides this virtual function to return a C string
 * describing its port counts.  The string gives acceptable input and output
 * ranges, separated by a slash.  Examples:
 *
 * <dl>
 * <dt><tt>"1/1"</tt></dt> <dd>The element has exactly one input port and one
 * output port.</dd>
 * <dt><tt>"1-2/0"</tt></dt> <dd>One or two input ports and zero output
 * ports.</dd>
 * <dt><tt>"1/-6"</tt></dt> <dd>One input port and up to six output ports.</dd>
 * <dt><tt>"2-/-"</tt></dt> <dd>At least two input ports and any number of
 * output ports.</dd>
 * <dt><tt>"3"</tt></dt> <dd>Exactly three input and output ports.  (If no
 * slash appears, the text is used for both input and output ranges.)</dd>
 * <dt><tt>"1-/="</tt></dt> <dd>At least one input port and @e the @e same
 * number of output ports.</dd>
 * <dt><tt>"1-/=+"</tt></dt> <dd>At least one input port and @e one @e more
 * output port than there are input ports.</dd>
 * </dl>
 *
 * Port counts are used to determine whether a configuration uses too few or
 * too many ports, and lead to errors such as "'e' has no input 3" and "'e'
 * input 3 unused".
 *
 * Click extracts port count specifiers from the source for use by tools.  For
 * Click to find a port count specifier, the function definition must appear
 * inline, on a single line, inside the element class's declaration, and must
 * return a C string constant (or a name below).  It should also have public
 * accessibility.  Here's an acceptable port_count() definition:
 *
 * @code
 * const char *port_count() const     { return "1/1"; }
 * @endcode
 *
 * The default port_count() method returns @c "0/0".
 *
 * The following names are available for common port count specifiers.
 *
 * @arg @c PORTS_0_0 for @c "0/0"
 * @arg @c PORTS_0_1 for @c "0/1"
 * @arg @c PORTS_1_0 for @c "1/0"
 * @arg @c PORTS_1_1 for @c "1/1"
 * @arg @c PORTS_1_1X2 for @c "1/1-2"
 *
 * Since port_count() should simply return a C string constant, it shouldn't
 * matter when it's called; nevertheless, it is called before configure().
 */
const char *
Element::port_count() const
{
    return PORTS_0_0;
}

static int
notify_nports_pair(const char *&s, const char *ends, int &lo, int &hi)
{
    if (s == ends || *s == '-')
	lo = 0;
    else if (isdigit((unsigned char) *s))
	s = cp_integer(s, ends, 10, &lo);
    else
	return -1;
    if (s < ends && *s == '-') {
	s++;
	if (s < ends && isdigit((unsigned char) *s))
	    s = cp_integer(s, ends, 10, &hi);
	else
	    hi = INT_MAX;
    } else
	hi = lo;
    return 0;
}

int
Element::notify_nports(int ninputs, int noutputs, ErrorHandler *errh)
{
    // Another version of this function is in tools/lib/processingt.cc.
    // Make sure you keep them in sync.
    const char *s_in = port_count();
    const char *s = s_in, *ends = s + strlen(s);
    int ninlo, ninhi, noutlo, nouthi, equal = 0;

    if (notify_nports_pair(s, ends, ninlo, ninhi) < 0)
	goto parse_error;

    if (s == ends)
	s = s_in;
    else if (*s == '/')
	s++;
    else
	goto parse_error;

    if (*s == '=') {
	const char *plus = s + 1;
	do {
	    equal++;
	} while (plus != ends && *plus++ == '+');
	if (plus != ends)
	    equal = 0;
    }
    if (!equal)
	if (notify_nports_pair(s, ends, noutlo, nouthi) < 0 || s != ends)
	    goto parse_error;

    if (ninputs < ninlo)
	ninputs = ninlo;
    else if (ninputs > ninhi)
	ninputs = ninhi;

    if (equal)
	noutputs = ninputs + equal - 1;
    else if (noutputs < noutlo)
	noutputs = noutlo;
    else if (noutputs > nouthi)
	noutputs = nouthi;

    set_nports(ninputs, noutputs);
    return 0;

  parse_error:
    if (errh)
	errh->error("%p{element}: bad port count", this);
    return -1;
}

void
Element::initialize_ports(const int *in_v, const int *out_v)
{
    for (int i = 0; i < ninputs(); i++) {
	// allowed iff in_v[i] == VPULL
	int port = (in_v[i] == VPULL ? 0 : -1);
	_ports[0][i].assign(false, this, 0, port);
    }

    for (int o = 0; o < noutputs(); o++) {
	// allowed iff out_v[o] != VPULL
	int port = (out_v[o] == VPULL ? -1 : 0);
	_ports[1][o].assign(true, this, 0, port);
    }
}

int
Element::connect_port(bool isoutput, int port, Element* e, int e_port)
{
    if (port_active(isoutput, port)) {
	_ports[isoutput][port].assign(isoutput, this, e, e_port);
	return 0;
    } else
	return -1;
}


// FLOW

/** @brief Return the element's internal packet flow specifier (its
 * <em>flow code</em>).
 *
 * An element class overrides this virtual function to return a C string
 * describing how packets flow within the element.  That is, can packets that
 * arrive on input port X be emitted on output port Y?  This information helps
 * Click answer questions such as "What Queues are downstream of this
 * element?" and "Should this agnostic port be push or pull?".  See below for
 * more.
 *
 * A flow code string consists of an input specification and an output
 * specification, separated by a slash.  Each specification is a sequence of
 * @e port @e codes.  Packets can travel from an input port to an output port
 * only if the port codes match.
 *
 * The simplest port code is a single case-sensitive letter.  For example, the
 * flow code @c "x/x" says that packets can travel from the element's input
 * port to its output port, while @c "x/y" says that packets never travel
 * between ports.
 *
 * A port code may also be a sequence of letters in brackets, such as
 * <tt>[abz]</tt>. Two port codes match iff they have at least one letter in
 * common, so <tt>[abz]</tt> matches <tt>a</tt>, but <tt>[abz]</tt> and
 * <tt>[cde]</tt> do not match. If a caret <tt>^</tt> appears after the open
 * bracket, the port code will match all letters @e except for
 * those after the caret. Thus, the port code <tt>[^bc]</tt> is equivalent to
 * <tt>[ABC...XYZadef...xyz]</tt>.
 *
 * Finally, the @c # character is also a valid port code, and may be used
 * within brackets.  One @c # matches another @c # only when they represent
 * the same port number -- for example, when one @c # corresponds to input
 * port 2 and the other to output port 2.  @c # never matches any letter.
 * Thus, for an element with exactly 2 inputs and 2 outputs, the flow code @c
 * "##/##" behaves like @c "xy/xy".
 *
 * The last code in each specification is duplicated as many times as
 * necessary, and any extra codes are ignored.  The flow codes @c
 * "[x#][x#][x#]/x######" and @c "[x#]/x#" behave identically.
 *
 * Here are some example flow codes.
 *
 * <dl>
 * <dt><tt>"x/x"</tt></dt>
 * <dd>Packets may travel from any input port to any output port.  Most
 * elements use this flow code.</dd>
 *
 * <dt><tt>"xy/x"</tt></dt>
 * <dd>Packets arriving on input port 0 may travel to any output port, but
 * those arriving on other input ports will not be emitted on any output.
 * @e ARPQuerier uses this flow code.</dd>
 *
 * <dt><tt>"x/y"</tt></dt> <dd>Packets never travel between input and output
 * ports. @e Idle and @e Error use this flow code.  So does @e KernelTun,
 * since its input port and output port are decoupled (packets received on its
 * input are sent to the kernel; packets received from the kernel are sent to
 * its output).</dd>
 *
 * <dt><tt>"#/#"</tt></dt> <dd>Packets arriving on input port @e K may travel
 * only to output port @e K.  @e Suppressor uses this flow code.</dd>
 *
 * <dt><tt>"#/[^#]"</tt></dt> <dd>Packets arriving on input port @e K may
 * travel to any output port except @e K.  @e EtherSwitch uses this flow
 * code.</dd>
 *
 * <dt><tt>"xy/[xy]x"</tt></dt> <dd>Packets arriving on input port 0 may
 * travel to any output port. Packet arriving on any other input port can
 * <em>only</em> travel to output port 0. @e Bypass uses this flow code.</dd>
 *
 * </dl>
 *
 * Click extracts flow codes from the source for use by tools.  For Click to
 * find a flow code, the function definition must appear inline, on a single
 * line, inside the element class's declaration, and must return a C string
 * constant.  It should also have public accessibility.  Here's an acceptable
 * flow_code() definition:
 *
 * @code
 * const char *flow_code() const     { return "xy/x"; }
 * @endcode
 *
 * The default flow_code() method returns @c "x/x", which indicates that
 * packets may travel from any input to any output.  This default is
 * acceptable for the vast majority of elements.
 *
 * The following name is available for a common flow code.
 *
 * @arg @c COMPLETE_FLOW for @c "x/x"
 *
 * Since flow_code() should simply return a C string constant, it shouldn't
 * matter when it's called; nevertheless, it is called before configure().
 *
 * <h3>Determining an element's flow code</h3>
 *
 * To pick the right flow code for an element, consider how a flow code would
 * affect a simple router.
 *
 * Given an element @e E with input port @e M and output port @e N, imagine
 * this simple configuration (or a similar configuration):
 *
 * <tt>... -> RED -> [@e M] E [@e N] -> Queue -> ...;</tt>
 *
 * Now, should the @e RED element include the @e Queue element in its queue
 * length calculation?  If so, then the flow code's <em>M</em>th input port
 * code and <em>N</em>th output port code should match.  If not, they
 * shouldn't.
 *
 * For example, consider @e ARPQuerier's second input port.  On receiving an
 * ARP response on that input, @e ARPQuerier may emit a held-over IP packet to
 * its first output.  However, a @e RED element upstream of that second input
 * port probably wouldn't count the downstream @e Queue in its queue length
 * calculation.  After all, the ARP responses are effectively dropped; packets
 * emitted onto the @e Queue originally came from @e ARPQuerier's first input
 * port.  Therefore, @e ARPQuerier's flow code, <tt>"xy/x"</tt>, specifies
 * that packets arriving on the second input port are not emitted on any
 * output port.
 *
 * The @e ARPResponder element provides a contrasting example.  It has one
 * input port, which receives ARP queries, and one output port, which emits
 * the corresponding ARP responses.  A @e RED element upstream of @e
 * ARPResponder probably @e would want to include a downstream @e Queue, since
 * queries received by @e ARPResponder are effectively transmuted into emitted
 * responses. Thus, @e ARPResponder's flow code, <tt>"x/x"</tt> (the default),
 * specifies that packets travel through it, even though the packets it emits
 * are completely different from the packets it receives.
 *
 * If you find this confusing, don't fret.  It is perfectly fine to be
 * conservative when assigning flow codes, and the vast majority of the Click
 * distribution's elements use @c COMPLETE_FLOW.
 */
const char *
Element::flow_code() const
{
    return COMPLETE_FLOW;
}

static void
skip_flow_code(const char*& p)
{
    if (*p != '/' && *p != 0) {
	if (*p == '[') {
	    for (p++; *p != ']' && *p; p++)
		/* nada */;
	    if (*p)
		p++;
	} else
	    p++;
    }
}

static int
next_flow_code(const char*& p, int port, Bitvector& code, ErrorHandler* errh, const Element* e)
{
    if (*p == '/' || *p == 0) {
	// back up to last code character
	if (p[-1] == ']') {
	    for (p -= 2; *p != '['; p--)
		/* nada */;
	} else
	    p--;
    }

    code.assign(256, false);

    if (*p == '[') {
	bool negated = false;
	if (p[1] == '^')
	    negated = true, p++;
	for (p++; *p != ']' && *p; p++) {
	    // no isalpha: avoid locale and signed char dependencies
	    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
		code[*p] = true;
	    else if (*p == '#')
		code[port + 128] = true;
	    else if (errh)
		errh->error("%<%p{element}%> flow code: invalid character %<%c%>", e, *p);
	}
	if (negated)
	    code.flip();
	if (!*p) {
	    if (errh)
		errh->error("%<%p{element}%> flow code: missing %<]%>", e);
	    p--;			// don't skip over final '\0'
	}
    } else if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
	code[*p] = true;
    else if (*p == '#')
	code[port + 128] = true;
    else {
	if (errh)
	    errh->error("%<%p{element}%> flow code: invalid character %<%c%>", e, *p);
	p++;
	return -1;
    }

    p++;
    return 0;
}

/** @brief Analyze internal packet flow with respect to port @a p.
 *
 * @param isoutput false for input ports, true for output ports
 * @param p port number
 * @param[out] travels the bitvector to initialize with internal packet flow
 * information
 *
 * Analyzes the element's flow_code() and determines how packets might travel
 * from the specified port.  The @a travels bitvector is initialized to have
 * one entry per complementary port; thus, if @a isoutput is true, then @a
 * travels has ninputs() entries.  The entry for port @a x is set to true iff
 * packets can travel from @a p to @a x.  Returns all false if @a p is
 * out of range.
 *
 * For example, if flow_code() is "xy/xxyx", and the element has 2 inputs and
 * 4 outputs, then:
 *
 *  - port_flow(false, 0, travels) returns [true, true, false, true]
 *  - port_flow(false, 1, travels) returns [false, false, true, false]
 *  - port_flow(true, 0, travels) returns [true, false]
 *
 * Uses an element's overridden flow code when one is supplied;
 * see Router::flow_code_override().
 *
 * @sa flow_code
 */
void
Element::port_flow(bool isoutput, int p, Bitvector* travels) const
{
    // Another version of this function is in tools/lib/processingt.cc.
    // Make sure you keep them in sync.
    const char *f = _router->flow_code_override(eindex());
    if (!f)
	f = flow_code();
    int nother = nports(!isoutput);
    if (p < 0 || p >= nports(isoutput)) {
	travels->assign(nother, false);
	return;
    } else if (!f || f == COMPLETE_FLOW) {
	travels->assign(nother, true);
	return;
    }

    travels->assign(nother, false);
    ErrorHandler* errh = ErrorHandler::default_handler();

    const char* f_in = f;
    const char* f_out = strchr(f, '/');
    f_out = (f_out ? f_out + 1 : f_in);
    if (*f_out == '\0' || *f_out == '/') {
	errh->error("%<%p{element}%> flow code: missing or bad %</%>", this);
	return;
    }

    if (isoutput) {
	const char* f_swap = f_in;
	f_in = f_out;
	f_out = f_swap;
    }

    Bitvector in_code;
    for (int i = 0; i < p; i++)
	skip_flow_code(f_in);
    next_flow_code(f_in, p, in_code, errh, this);

    Bitvector out_code;
    for (int i = 0; i < nother; i++) {
	next_flow_code(f_out, i, out_code, errh, this);
	if (in_code.nonzero_intersection(out_code))
	    (*travels)[i] = true;
    }
}


// PUSH OR PULL PROCESSING

/** @brief Return the element's processing specifier.
 *
 * An element class overrides this virtual function to return a C string
 * describing which of its ports are push, pull, or agnostic.  The string
 * gives acceptable input and output ranges, separated by a slash; the
 * characters @c "h", @c "l", and @c "a" indicate push, pull, and agnostic
 * ports, respectively.  Examples:
 *
 * @arg @c "h/h" All input and output ports are push.
 * @arg @c "h/l" Push input ports and pull output ports.
 * @arg @c "a/ah" All input ports are agnostic.  The first output port is also
 * agnostic, but the second and subsequent output ports are push.
 * @arg @c "hl/hlh" Input port 0 and output port 0 are push.  Input port 1 and
 * output port 1 are pull.  All remaining inputs are pull; all remaining
 * outputs are push.
 * @arg @c "a" All input and output ports are agnostic.  (If no slash appears,
 * the text is used for both input and output ports.)
 *
 * Thus, each character indicates a single port's processing type, except that
 * the last character in the input section is used for all remaining input
 * ports (and similarly for outputs).  It's OK to have more characters than
 * ports; any extra characters are ignored.
 *
 * Click extracts processing specifiers from the source for use by tools.  For
 * Click to find a processing specifier, the function definition must appear
 * inline, on a single line, inside the element class's declaration, and must
 * return a C string constant.  It should also have public accessibility.
 * Here's an acceptable processing() definition:
 *
 * @code
 * const char *processing() const     { return "a/ah"; }
 * @endcode
 *
 * The default processing() method returns @c "a/a", which sets all ports to
 * agnostic.
 *
 * The following names are available for common processing specifiers.
 *
 * @arg @c AGNOSTIC for @c "a/a"
 * @arg @c PUSH for @c "h/h"
 * @arg @c PULL for @c "l/l"
 * @arg @c PUSH_TO_PULL for @c "h/l"
 * @arg @c PULL_TO_PUSH for @c "l/h"
 * @arg @c PROCESSING_A_AH for @c "a/ah"
 *
 * Since processing() should simply return a C string constant, it shouldn't
 * matter when it's called; nevertheless, it is called before configure().
 */
const char*
Element::processing() const
{
    return AGNOSTIC;
}

int
Element::next_processing_code(const char*& p, ErrorHandler* errh)
{
    switch (*p) {

      case 'h': case 'H':
	p++;
	return Element::VPUSH;

      case 'l': case 'L':
	p++;
	return Element::VPULL;

      case 'a': case 'A':
	p++;
	return Element::VAGNOSTIC;

      case '/': case 0:
	return -2;

      default:
	if (errh)
	    errh->error("bad processing code");
	p++;
	return -1;

    }
}

void
Element::processing_vector(int* in_v, int* out_v, ErrorHandler* errh) const
{
    const char* p_in = processing();
    int val = 0;

    const char* p = p_in;
    int last_val = 0;
    for (int i = 0; i < ninputs(); i++) {
	if (last_val >= 0)
	    last_val = next_processing_code(p, errh);
	if (last_val >= 0)
	    val = last_val;
	in_v[i] = val;
    }

    while (*p && *p != '/')
	p++;
    if (!*p)
	p = p_in;
    else
	p++;

    last_val = 0;
    for (int i = 0; i < noutputs(); i++) {
	if (last_val >= 0)
	    last_val = next_processing_code(p, errh);
	if (last_val >= 0)
	    val = last_val;
	out_v[i] = val;
    }
}

/** @brief Return the element's flags.
 *
 * @warning This interface is not stable.
 *
 * This virtual function is called to fetch a string describing the element's
 * flags.  A flags word includes one or more space-separated flag settings,
 * where a flag setting consists of an uppercase letter optionally followed by
 * a number.  The following flags are currently defined.
 *
 * <dl>
 *
 * <dt><tt>A</tt></dt> <dd>This element requires AlignmentInfo information.
 * The click-align tool only generates AlignmentInfo for <tt>A</tt>-flagged
 * elements.</dd>
 *
 * <dt><tt>S0</tt></dt> <dd>This element neither generates nor consumes
 * packets.  In other words, every packet received on its inputs will be
 * emitted on its outputs, and every packet emitted on its outputs must have
 * originated from its inputs.  Notification uses this flag to discover
 * certain idle paths.  For example, packet schedulers (RoundRobinSched,
 * PrioSched) never generate packets and so declare the <tt>S0</tt> flag.  As
 * a result, degenerate paths like "RoundRobinSched -> ToDevice", where
 * RoundRobinSched has 0 inputs, are idle rather than busy, and waste no
 * CPU time.</dd>
 *
 * </dl>
 */
const char*
Element::flags() const
{
    return "";
}

/** @brief Return the flag value for @a flag in flags().
 * @param flag the flag
 *
 * Returns the numeric flag value if @a flag was specified in flags(), 1 if @a
 * flag was specified without a numeric flag value, and -1 if @a flag was not
 * specified. */
int
Element::flag_value(int flag) const
{
    assert(flag > 0 && flag < 256);
    for (const unsigned char *data = reinterpret_cast<const unsigned char *>(flags()); *data; ++data)
	if (*data == flag) {
	    if (data[1] && isdigit(data[1])) {
		int value = 0;
		for (++data; isdigit(*data); ++data)
		    value = 10 * value + *data - '0';
		return value;
	    } else
		return 1;
	} else
	    while (isspace(*data))
		++data;
    return -1;
}

// CLONING AND CONFIGURING

/** @brief Return the element's configure phase, which determines the
 * order in which elements are configured and initialized.
 *
 * Click configures and initializes elements in increasing order of
 * configure_phase().  An element with configure phase 1 will always be
 * configured (have its configure() method called) before an element with
 * configure phase 2.  Thus, if two element classes must be configured in a
 * given order, they should define configure_phase() functions to enforce that
 * order.  For example, the @e AddressInfo element defines address
 * abbreviations for other elements to use; it should thus be configured
 * before other elements, and its configure_phase() method returns a low
 * value.
 *
 * Configure phases should be defined relative to the following constants,
 * which are listed in increasing order.
 *
 * <dl>
 * <dt><tt>CONFIGURE_PHASE_FIRST</tt></dt>
 * <dd>Configure before other elements.  Used by @e AddressInfo.</dd>
 *
 * <dt><tt>CONFIGURE_PHASE_INFO</tt></dt>
 * <dd>Configure early.  Appropriate for most information elements, such as @e ScheduleInfo.</dd>
 *
 * <dt><tt>CONFIGURE_PHASE_PRIVILEGED</tt></dt>
 * <dd>Intended for elements that require root
 * privilege when run at user level, such as @e FromDevice and
 * @e ToDevice.  The @e ChangeUID element, which reliquishes root
 * privilege, runs at configure phase @c CONFIGURE_PHASE_PRIVILEGED + 1.</dd>
 *
 * <dt><tt>CONFIGURE_PHASE_DEFAULT</tt></dt> <dd>The default implementation
 * returns @c CONFIGURE_PHASE_DEFAULT, so most elements are configured at this
 * phase.  Appropriate for most elements.</dd>
 *
 * <dt><tt>CONFIGURE_PHASE_LAST</tt></dt>
 * <dd>Configure after other elements.</dd>
 * </dl>
 *
 * The body of a configure_phase() method should consist of a single @c return
 * statement returning some constant.  Although it shouldn't matter when it's
 * called, it is called before configure().
 */
int
Element::configure_phase() const
{
    return CONFIGURE_PHASE_DEFAULT;
}

/** @brief Parse the element's configuration arguments.
 *
 * @param conf configuration arguments
 * @param errh error handler
 *
 * The configure() method is passed the element's configuration arguments.  It
 * should parse them, report any errors, and initialize the element's internal
 * state.
 *
 * The @a conf argument is the element's configuration string, divided into
 * configuration arguments by splitting at commas and removing comments and
 * leading and trailing whitespace (see cp_argvec()).  If @a conf is empty,
 * the element was not supplied with a configuration string (or its
 * configuration string contained only comments and whitespace).  It is safe
 * to modify @a conf; modifications will be thrown away when the function
 * returns.
 *
 * Any errors, warnings, or messages should be reported to @a errh.  Messages
 * need not specify the element name or type, since this information will be
 * provided as context.  @a errh.nerrors() is initially zero.
 *
 * configure() should return a negative number if configuration fails.
 * Returning a negative number prevents the router from initializing.  The
 * default configure() method succeeds if and only if there are no
 * configuration arguments.
 *
 * configure() methods are called in order of configure_phase().  All
 * elements' configure() methods are called, even if an early configure()
 * method fails; this is to report all relevant error messages to the user,
 * rather than just the first.
 *
 * configure() is called early in the initialization process, and cannot check
 * whether a named handler exists.  That function must be left for
 * initialize().  Assuming all router connections are valid and all
 * configure() methods succeed, the add_handlers() functions will be called
 * next.
 *
 * A configure() method should avoid potentially harmful actions, such
 * as truncating files or attaching to devices.  These actions should be left
 * for the initialize() method, which is called later.  This avoids harm if
 * another element cannot be configured, or if the router is incorrectly
 * connected, since in those cases initialize() will never be called.
 *
 * Elements that support live reconfiguration (see can_live_reconfigure())
 * should expect configure() to be called at run time, when a user writes to
 * the element's @c config handler.  In that case, configure() must be careful
 * not to disturb the existing configuration unless the new configuration is
 * error-free.
 *
 * @note In previous releases, configure() could not determine whether a port
 * is push or pull or query the router for information about neighboring
 * elements.  Those functions had to be left for initialize().  Even in the
 * current release, if any element in a configuration calls the deprecated
 * set_ninputs() or set_noutputs() function from configure(), then all push,
 * pull, and neighbor information is invalidated until initialize() time.
 *
 * @sa live_reconfigure, args.hh for argument parsing
 */
int
Element::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).complete();
}

/** @brief Install the element's handlers.
 *
 * The add_handlers() method should install any handlers the element provides
 * by calling add_read_handler(), add_write_handler(), and set_handler().
 * These functions may also be called from configure(), initialize(), or even
 * later, during router execution.  However, it is better in most cases to
 * initialize handlers in configure() or add_handlers(), since elements that
 * depend on other handlers often check in initialize() whether those handlers
 * exist.
 *
 * add_handlers() is called after configure() and before initialize().  When
 * it runs, it is guaranteed that every configure() method succeeded and that
 * all connections are correct (push and pull match up correctly and there are
 * no unused or badly-connected ports).
 *
 * Most add_handlers() methods simply call add_read_handler(),
 * add_write_handler(), add_task_handlers(), and possibly set_handler() one or
 * more times.  The default add_handlers() method does nothing.
 *
 * Click automatically provides five handlers for each element: @c class, @c
 * name, @c config, @c ports, and @c handlers.  There is no need to provide
 * these yourself.
 */
void
Element::add_handlers()
{
}

/** @brief Initialize the element.
 *
 * @param errh error handler
 *
 * The initialize() method is called just before the router is placed on
 * line. It performs any final initialization, and provides the last chance to
 * abort router installation with an error.  Any errors, warnings, or messages
 * should be reported to @a errh.  Messages need not specify the element
 * name; this information will be supplied externally.  @a errh.nerrors()
 * is initially zero.
 *
 * initialize() should return zero if initialization succeeds, or a negative
 * number if it fails.  Returning a negative number prevents the router from
 * initializing.  The default initialize() method always returns zero
 * (success).
 *
 * initialize() methods are called in order of configure_phase(), using the
 * same order as for configure().  When an initialize() method fails, router
 * initialization stops immediately, and no more initialize() methods are
 * called.  Thus, at most one initialize() method can fail per router
 * configuration.
 *
 * initialize() is called after add_handlers() and before take_state().  When
 * it runs, it is guaranteed that every configure() method succeeded, that all
 * connections are correct (push and pull match up correctly and there are no
 * unused or badly-connected ports), and that every add_handlers() method has
 * been called.
 *
 * If every element's initialize() method succeeds, then the router is
 * installed, and will remain installed until another router replaces it.  Any
 * errors that occur later than initialize() -- during take_state(), push(),
 * or pull(), for example -- will not take the router off line.
 *
 * Strictly speaking, the only task that @e must go in initialize() is
 * checking whether a handler exists, since that information isn't available
 * at configure() time.  It's often convenient, however, to put other
 * functionality in initialize().  For example, opening files for writing fits
 * well in initialize(): if the configuration has errors before the relevant
 * element is initialized, any existing file will be left as is.  Common tasks
 * performed in initialize() methods include:
 *
 *   - Initializing Task objects.
 *   - Allocating memory.
 *   - Opening files.
 *   - Initializing network devices.
 *
 * @note initialize() methods may not create or destroy input and output
 * ports, but this functionality is deprecated anyway.
 *
 * @note In previous releases, configure() could not determine whether a port
 * was push or pull or query the router for information about neighboring
 * elements, so those tasks were relegated to initialize() methods.  In the
 * current release, configure() can perform these tasks too.
 */
int
Element::initialize(ErrorHandler *errh)
{
    (void) errh;
    return 0;
}

/** @brief Initialize the element for hotswap, where the element should take
 * @a old_element's state, if possible.
 *
 * @param old_element element in the old configuration; result of
 * hotswap_element()
 * @param errh error handler
 *
 * The take_state() method supports hotswapping, and is the last stage of
 * configuration installation.  When a configuration is successfully installed
 * with the hotswap option, the driver (1) stops the old configuration, (2)
 * searches the two configurations for pairs of compatible elements, (3) calls
 * take_state() on the new elements in those pairs to give them a chance to
 * take state from the old elements, and (4) starts the new configuration.
 *
 * take_state() is called only when a configuration is hotswapped in.  The
 * default take_state() implementation does nothing; there's no need to
 * override it unless your element has state you want preserved across
 * hotswaps.
 *
 * The @a old_element argument is an element from the old configuration (that
 * is, from router()->@link Router::hotswap_router() hotswap_router()@endlink)
 * obtained by calling hotswap_element().  If hotswap_element() returns null,
 * take_state() will not be called.  The default hotswap_element() returns an
 * @a old_element has the same name() as this element.  This is often too
 * loose; for instance, @a old_element might have a completely different
 * class.  Thus, most take_state() methods begin by attempting to cast() @a
 * old_element to a compatible class, and silently returning if the result is
 * null.  Alternatively, you can override hotswap_element() and put the check
 * there.
 *
 * Errors and warnings should be reported to @a errh, but the router will be
 * installed whether or not there are errors.  take_state() should always
 * leave this element in a state that's safe to run, and @a old_element in a
 * state that's safe to cleanup().
 *
 * take_state() is called after initialize().  When it runs, it is guaranteed
 * that this element's configuration will shortly be installed.  Every
 * configure() and initialize() method succeeded, all connections are correct
 * (push and pull match up correctly and there are no unused or
 * badly-connected ports), and every add_handlers() method has been called.
 * It is also guaranteed that the old configuration (of which old_element is a
 * part) had been successfully installed, but that none of its tasks are
 * running at the moment.
 */
void
Element::take_state(Element *old_element, ErrorHandler *errh)
{
    (void) old_element, (void) errh;
}

/** @brief Return a compatible element in the hotswap router.
 *
 * hotswap_element() searches the hotswap router, router()->@link
 * Router::hotswap_router() hotswap_router()@endlink, for an element
 * compatible with this element.  It returns that element, if any.  If there's
 * no compatible element, or no hotswap router, then it returns 0.
 *
 * The default implementation searches for an element with the same name as
 * this element.  Thus, it returns 0 or an element that satisfies this
 * constraint: hotswap_element()->name() == name().
 *
 * Generally, this constraint is too loose.  A @e Queue element can't hotswap
 * state from an @e ARPResponder, even if they do have the same name.  Most
 * elements also check that hotswap_element() has the right class, using the
 * cast() function.  This check can go either in hotswap_element() or in
 * take_state() itself, whichever is easier; Click doesn't use the result of
 * hotswap_element() except as an argument to take_state().
 *
 * @sa take_state, Router::hotswap_router
 */
Element *
Element::hotswap_element() const
{
    if (Router *r = router()->hotswap_router())
	if (Element *e = r->find(name()))
	    return e;
    return 0;
}

/** @brief Clean up the element's state.
 *
 * @param stage this element's maximum initialization stage
 *
 * The cleanup() method should clean up any state allocated by the
 * initialization process.  For example, it should close any open files, free
 * up memory, and unhook from network devices.  Click calls cleanup() when it
 * determines that an element's state is no longer needed, either because a
 * router configuration is about to be removed or because the router
 * configuration failed to initialize properly.  Click will call the cleanup()
 * method exactly once on every element it creates.
 *
 * The @a stage parameter is an enumeration constant indicating how far the
 * element made it through the initialization process.  Possible values are,
 * in increasing order:
 *
 * <dl>
 * <dt><tt>CLEANUP_BEFORE_CONFIGURE</tt></dt>
 * <dd>The element's configure() method was not called.  This happens when
 *   some element's port counts or push/pull processing was wrong.</dd>
 *
 * <dt><tt>CLEANUP_CONFIGURE_FAILED</tt></dt>
 * <dd>The element's configure() method was called, but it failed.</dd>
 *
 * <dt><tt>CLEANUP_CONFIGURED</tt></dt> <dd>The element's configure() method
 * was called and succeeded, but its initialize() method was not called
 * (because some other element's configure() method failed, or there was a
 * problem with the configuration's connections).</dd>
 *
 * <dt><tt>CLEANUP_INITIALIZE_FAILED</tt></dt> <dd>The element's configure()
 * and initialize() methods were both called.  configure() succeeded, but
 * initialize() failed.</dd>
 *
 * <dt><tt>CLEANUP_INITIALIZED</tt></dt> <dd>The element's configure() and
 * initialize() methods were called and succeeded, but its router was never
 * installed (because some other element's initialize() method failed).</dd>
 *
 * <dt><tt>CLEANUP_ROUTER_INITIALIZED</tt></dt> <dd>The element's configure()
 * and initialize() methods were called and succeeded, and the router of which
 * it is a part was successfully installed.</dd>
 *
 * <dt><tt>CLEANUP_MANUAL</tt></dt> <dd>Never used by Click.  Intended for use
 * when element code calls cleanup() explicitly.</dd>
 * </dl>
 *
 * A configuration's cleanup() methods are called in the reverse of the
 * configure_phase() order used for configure() and initialize().
 *
 * The default cleanup() method does nothing.
 *
 * cleanup() serves some of the same functions as an element's destructor.
 * However, cleanup() may be called long before an element is destroyed.
 * Elements that are part of an erroneous router are cleaned up, but kept
 * around for debugging purposes until another router is installed.
 */
void
Element::cleanup(CleanupStage stage)
{
    (void) stage;
}


// LIVE CONFIGURATION

/** @brief Return whether an element supports live reconfiguration.
 *
 * Returns true iff this element can be reconfigured as the router is running.
 * Click will make the element's "config" handler writable if
 * can_live_reconfigure() returns true; when that handler is written, Click
 * will call the element's live_reconfigure() function.  The default
 * implementation returns false.
 */
bool
Element::can_live_reconfigure() const
{
  return false;
}

/** @brief Reconfigure the element while the router is running.
 *
 * @param conf configuration arguments
 * @param errh error handler
 *
 * This function should parse the configuration arguments in @a conf, set the
 * element's state accordingly, and report any error messages or warnings to
 * @a errh.  This resembles configure().  However, live_reconfigure() is
 * called when the element's "config" handler is written, rather than at
 * router initialization time.  Thus, the element already has a working
 * configuration.  If @a conf has an error, live_reconfigure() should leave
 * this previous working configuration alone.
 *
 * can_live_reconfigure() must return true for live_reconfigure() to work.
 *
 * Return >= 0 on success, < 0 on error.  On success, Click will set the
 * element's old configuration arguments to @a conf, so that later reads of
 * the "config" handler will return @a conf.  (An element can override this
 * by defining its own "config" handler.)
 *
 * The default implementation simply calls configure(@a conf, @a errh).  This
 * is OK as long as configure() doesn't change the element's state on error.
 *
 * @sa can_live_reconfigure
 */
int
Element::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
  if (can_live_reconfigure())
    return configure(conf, errh);
  else
    return errh->error("cannot reconfigure %p{element} live", this);
}


/** @brief Return the element's current configuration string.
 *
 * The configuration string is obtained by calling the element's "config"
 * read handler.  The default read handler calls Router::econfiguration().
 */
String
Element::configuration() const
{
    if (const Handler *h = router()->handler(this, "config"))
	if (h->readable())
	    return h->call_read(const_cast<Element *>(this), 0);
    return router()->econfiguration(eindex());
}


/** @brief Return the element's home thread. */
RouterThread *
Element::home_thread() const
{
    return master()->thread(router()->home_thread_id(this));
}


// SELECT

#if CLICK_USERLEVEL

/** @brief Handle a file descriptor event.
 *
 * @param fd the file descriptor
 * @param mask relevant events: bitwise-or of one or more of SELECT_READ, SELECT_WRITE
 *
 * Click's call to select() indicates that the file descriptor @a fd is
 * readable, writable, or both.  The overriding method should read or write
 * the file descriptor as appropriate.  The default implementation causes an
 * assertion failure.
 *
 * The element must have previously registered interest in @a fd with
 * add_select().
 *
 * @note Only available at user level.
 *
 * @sa add_select, remove_select
 */
void
Element::selected(int fd, int mask)
{
    (void) mask;
    selected(fd);
}

/** @brief Handle a file descriptor event.
 *
 * @param fd the file descriptor
 *
 * @deprecated Elements should define selected(@a fd, mask) in preference to
 * selected(@a fd).  The default implementation of selected(@a fd, mask) calls
 * selected(@a fd) for backwards compatibility.
 *
 * @note Only available at user level.
 *
 * @sa add_select, remove_select
 */
void
Element::selected(int fd)
{
    assert(0 /* selected not overridden */);
    (void) fd;
}

/** @brief Register interest in @a mask events on file descriptor @a fd.
 *
 * @param fd the file descriptor
 * @param mask relevant events: bitwise-or of one or more of SELECT_READ, SELECT_WRITE
 *
 * Click will register interest in readability and/or writability on file
 * descriptor @a fd.  When @a fd is ready, Click will call this element's
 * selected(@a fd, @a mask) method.
 *
 * add_select(@a fd, @a mask) overrides any previous add_select() for the same
 * @a fd and events in @a mask.  However, different elements may register
 * interest in different events for the same @a fd.
 *
 * @note Only available at user level.
 *
 * @note Selecting for writability with SELECT_WRITE normally requires more
 * care than selecting for readability with SELECT_READ.  You should
 * add_select(@a fd, SELECT_WRITE) only when there is data to write to @a fd.
 * Otherwise, Click will constantly poll your element's selected(@a fd, @a
 * mask) method.
 *
 * @sa remove_select, selected
 */
int
Element::add_select(int fd, int mask)
{
    return home_thread()->select_set().add_select(fd, this, mask);
}

/** @brief Remove interest in @a mask events on file descriptor @a fd.
 *
 * @param fd the file descriptor
 * @param mask relevant events: bitwise-or of one or more of SELECT_READ, SELECT_WRITE
 *
 * Click will remove any existing add_select() registrations for readability
 * and/or writability on file descriptor @a fd.  The named events on @a fd
 * will no longer cause a selected() call.
 *
 * @note Only available at user level.
 *
 * @sa add_select, selected
 */
int
Element::remove_select(int fd, int mask)
{
    return home_thread()->select_set().remove_select(fd, this, mask);
}

#endif


// HANDLERS

/** @brief Register a read handler named @a name.
 *
 * @param name handler name
 * @param read_callback function called when handler is read
 * @param user_data user data parameter passed to @a read_callback
 * @param flags flags to set
 *
 * Adds a read handler named @a name for this element.  Reading the handler
 * returns the result of the @a read_callback function, which is called like
 * this:
 *
 * @code
 * String result = read_callback(e, user_data);
 * @endcode
 *
 * @a e is this element pointer.
 *
 * add_read_handler(@a name) overrides any previous
 * add_read_handler(@a name) or set_handler(@a name), but any previous
 * add_write_handler(@a name) remains in effect.
 *
 * The added read handler takes no parameters.  To create a read handler with
 * parameters, use set_handler() or set_handler_flags().
 *
 * @sa read_positional_handler, read_keyword_handler: standard read handler
 * callback functions
 * @sa add_write_handler, set_handler, add_task_handlers
 */
void
Element::add_read_handler(const String &name, ReadHandlerCallback read_callback, const void *user_data, uint32_t flags)
{
    Router::add_read_handler(this, name, read_callback, (void *) user_data, flags);
}

/** @brief Register a read handler named @a name.
 *
 * This version of add_read_handler() is useful when @a user_data is an
 * integer.  Note that the @a read_callback function must still cast its
 * <tt>void *</tt> argument to <tt>intptr_t</tt> to obtain the integer value.
 */
void
Element::add_read_handler(const String &name, ReadHandlerCallback read_callback, int user_data, uint32_t flags)
{
    uintptr_t u = (uintptr_t) user_data;
    Router::add_read_handler(this, name, read_callback, (void *) u, flags);
}

/** @brief Register a read handler named @a name.
 *
 * This version of add_read_handler() is useful when @a name is a static
 * constant string.  @a name is passed to String::make_stable.  The memory
 * referenced by @a name must remain valid for as long as the router containing
 * this element.
 */
void
Element::add_read_handler(const char *name, ReadHandlerCallback read_callback, int user_data, uint32_t flags)
{
    uintptr_t u = (uintptr_t) user_data;
    Router::add_read_handler(this, String::make_stable(name), read_callback, (void *) u, flags);
}

/** @brief Register a write handler named @a name.
 *
 * @param name handler name
 * @param write_callback function called when handler is written
 * @param user_data user data parameter passed to @a write_callback
 * @param flags flags to set
 *
 * Adds a write handler named @a name for this element.  Writing the handler
 * calls the @a write_callback function like this:
 *
 * @code
 * int r = write_callback(data, e, user_data, errh);
 * @endcode
 *
 * @a e is this element pointer.  The return value @a r should be negative on
 * error, positive or zero on success.  Any messages should be reported to the
 * @a errh ErrorHandler object.
 *
 * add_write_handler(@a name) overrides any previous
 * add_write_handler(@a name) or set_handler(@a name), but any previous
 * add_read_handler(@a name) remains in effect.
 *
 * @sa reconfigure_positional_handler, reconfigure_keyword_handler: standard
 * write handler callback functions
 * @sa add_read_handler, set_handler, add_task_handlers
 */
void
Element::add_write_handler(const String &name, WriteHandlerCallback write_callback, const void *user_data, uint32_t flags)
{
    Router::add_write_handler(this, name, write_callback, (void *) user_data, flags);
}

/** @brief Register a write handler named @a name.
 *
 * This version of add_write_handler() is useful when @a user_data is an
 * integer.  Note that the @a write_callback function must still cast its
 * <tt>void *</tt> argument to <tt>intptr_t</tt> to obtain the integer value.
 */
void
Element::add_write_handler(const String &name, WriteHandlerCallback write_callback, int user_data, uint32_t flags)
{
    uintptr_t u = (uintptr_t) user_data;
    Router::add_write_handler(this, name, write_callback, (void *) u, flags);
}

/** @brief Register a write handler named @a name.
 *
 * This version of add_write_handler() is useful when @a name is a static
 * constant string.  @a name is passed to String::make_stable.  The memory
 * referenced by @a name must remain valid for as long as the router containing
 * this element.
 */
void
Element::add_write_handler(const char *name, WriteHandlerCallback write_callback, int user_data, uint32_t flags)
{
    uintptr_t u = (uintptr_t) user_data;
    Router::add_write_handler(this, String::make_stable(name), write_callback, (void *) u, flags);
}

/** @brief Register a comprehensive handler named @a name.
 *
 * @param name handler name
 * @param flags handler flags
 * @param callback function called when handler is written
 * @param read_user_data read user data parameter stored in the handler
 * @param write_user_data write user data parameter stored in the handler
 *
 * Registers a comprehensive handler named @a name for this element.  The
 * handler handles the operations specified by @a flags, which can include
 * Handler::h_read, Handler::h_write, Handler::h_read_param, and others.
 * Reading the handler calls the @a callback function like this:
 *
 * @code
 * String data;
 * int r = callback(Handler::h_read, data, e, h, errh);
 * @endcode
 *
 * Writing the handler calls it like this:
 *
 * @code
 * int r = callback(Handler::h_write, data, e, h, errh);
 * @endcode
 *
 * @a e is this element pointer, and @a h points to the Handler object for
 * this handler.  The @a data string is an out parameter for reading and an in
 * parameter for writing; when reading with parameters, @a data has the
 * parameters on input and should be replaced with the result on output.  The
 * return value @a r should be negative on error, positive or zero on success.
 * Any messages should be reported to the @a errh ErrorHandler object.
 *
 * set_handler(@a name) overrides any previous
 * add_read_handler(@a name), add_write_handler(@a name), or set_handler(@a
 * name).
 */
void
Element::set_handler(const String& name, int flags, HandlerCallback callback, const void *read_user_data, const void *write_user_data)
{
    Router::set_handler(this, name, flags, callback, (void *) read_user_data, (void *) write_user_data);
}

/** @brief Register a comprehensive handler named @a name.
 *
 * This version of set_handler() is useful when @a user_data is an integer.
 * Note that the Handler::user_data() methods still return <tt>void *</tt>
 * values.
 */
void
Element::set_handler(const String &name, int flags, HandlerCallback callback, int read_user_data, int write_user_data)
{
    uintptr_t u1 = (uintptr_t) read_user_data, u2 = (uintptr_t) write_user_data;
    Router::set_handler(this, name, flags, callback, (void *) u1, (void *) u2);
}

/** @brief Register a comprehensive handler named @a name.
 *
 * This version of set_handler() is useful when @a name is a static
 * constant string.  @a name is passed to String::make_stable.  The memory
 * referenced by @a name must remain valid for as long as the router containing
 * this element.
 */
void
Element::set_handler(const char *name, int flags, HandlerCallback callback, int read_user_data, int write_user_data)
{
    uintptr_t u1 = (uintptr_t) read_user_data, u2 = (uintptr_t) write_user_data;
    Router::set_handler(this, String::make_stable(name), flags, callback, (void *) u1, (void *) u2);
}

/** @brief Set flags for the handler named @a name.
 * @param name handler name
 * @param set_flags handler flags to set
 * @param clear_flags handler flags to clear
 *
 * Sets flags for any handlers named @a name for this element.  Fails if no @a
 * name handler exists.
 */
int
Element::set_handler_flags(const String& name, int set_flags, int clear_flags)
{
    return Router::set_handler_flags(this, name, set_flags, clear_flags);
}

static String
read_class_handler(Element *e, void *)
{
    return String(e->class_name());
}

static String
read_name_handler(Element *e, void *)
{
    return e->name();
}

static String
read_config_handler(Element *e, void *)
{
    return e->router()->econfiguration(e->eindex());
}

static int
write_config_handler(const String &str, Element *e, void *,
		     ErrorHandler *errh)
{
    Vector<String> conf;
    cp_argvec(str, conf);
    int r = e->live_reconfigure(conf, errh);
    if (r >= 0)
	e->router()->set_econfiguration(e->eindex(), str);
    return r;
}

static String
read_ports_handler(Element *e, void *)
{
    return e->router()->element_ports_string(e);
}

String
Element::read_handlers_handler(Element *e, void *)
{
    Vector<int> hindexes;
    Router::element_hindexes(e, hindexes);
    StringAccum sa;
    for (int* hip = hindexes.begin(); hip < hindexes.end(); hip++) {
	const Handler* h = Router::handler(e->router(), *hip);
	if (h->read_visible() || h->write_visible()) {
	    sa << h->name() << '\t';
	    if (h->read_visible())
		sa << 'r';
	    if (h->write_visible())
		sa << 'w';
	    if (h->read_param())
		sa << '+';
	    if (h->flags() & Handler::h_raw)
		sa << '%';
	    if (h->flags() & Handler::h_calm)
		sa << '.';
	    if (h->flags() & Handler::h_expensive)
		sa << '$';
	    if (h->flags() & Handler::h_uncommon)
		sa << 'U';
	    if (h->flags() & Handler::h_deprecated)
		sa << 'D';
	    if (h->flags() & Handler::h_button)
		sa << 'b';
	    if (h->flags() & Handler::h_checkbox)
		sa << 'c';
	    sa << '\n';
	}
    }
    return sa.take_string();
}


#if CLICK_STATS >= 1

static String
read_icounts_handler(Element *f, void *)
{
  StringAccum sa;
  for (int i = 0; i < f->ninputs(); i++)
    if (f->input(i).active() || CLICK_STATS >= 2)
      sa << f->input(i).npackets() << "\n";
    else
      sa << "??\n";
  return sa.take_string();
}

static String
read_ocounts_handler(Element *f, void *)
{
  StringAccum sa;
  for (int i = 0; i < f->noutputs(); i++)
    if (f->output(i).active() || CLICK_STATS >= 2)
      sa << f->output(i).npackets() << "\n";
    else
      sa << "??\n";
  return sa.take_string();
}

#endif /* CLICK_STATS >= 1 */

#if CLICK_STATS >= 2
String
Element::read_cycles_handler(Element *e, void *)
{
    StringAccum sa;
    if (e->_task_calls)
	sa << "tasks " << e->_task_calls << ' ' << e->_task_own_cycles << '\n';
    if (e->_timer_calls)
	sa << "timers " << e->_timer_calls << ' ' << e->_timer_own_cycles << '\n';
    if (e->_xfer_calls)
	sa << "xfer " << e->_xfer_calls << ' ' << e->_xfer_own_cycles << '\n';
    return sa.take_string();
}

int
Element::write_cycles_handler(const String &, Element *e, void *, ErrorHandler *)
{
    e->reset_cycles();
    return 0;
}
#endif

void
Element::add_default_handlers(bool allow_write_config)
{
  add_read_handler("name", read_name_handler, 0, Handler::h_calm);
  add_read_handler("class", read_class_handler, 0, Handler::h_calm);
  add_read_handler("config", read_config_handler, 0, Handler::h_calm);
  if (allow_write_config && can_live_reconfigure())
    add_write_handler("config", write_config_handler, 0);
  add_read_handler("ports", read_ports_handler, 0, Handler::h_calm);
  add_read_handler("handlers", read_handlers_handler, 0, Handler::h_calm);
#if CLICK_STATS >= 1
  add_read_handler("icounts", read_icounts_handler, 0);
  add_read_handler("ocounts", read_ocounts_handler, 0);
# if CLICK_STATS >= 2
  add_read_handler("cycles", read_cycles_handler, 0);
  add_write_handler("cycles", write_cycles_handler, 0);
# endif
#endif
}

#if HAVE_STRIDE_SCHED
static String
read_task_tickets(Element *e, void *thunk)
{
  Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
  return String(task->tickets());
}

static int
write_task_tickets(const String &s, Element *e, void *thunk, ErrorHandler *errh)
{
  Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
  int tix;
  if (!IntArg().parse_saturating(s, tix))
    return errh->error("syntax error");
  if (tix < 1) {
    errh->warning("tickets pinned at 1");
    tix = 1;
  } else if (tix > Task::MAX_TICKETS) {
    errh->warning("tickets pinned at %d", Task::MAX_TICKETS);
    tix = Task::MAX_TICKETS;
  }
  task->set_tickets(tix);
  return 0;
}
#endif

static String
read_task_scheduled(Element *e, void *thunk)
{
    Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
#if CLICK_DEBUG_SCHEDULING
    StringAccum sa;
    sa << task->scheduled();
    if (!task->on_scheduled_list() && task->scheduled()) {
	sa << " /* not on list";
	if (task->on_pending_list())
	    sa << ", on pending";
	sa << " */";
    }
    return sa.take_string();
#else
    return String(task->scheduled());
#endif
}

static int
write_task_scheduled(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
    bool scheduled;
    if (!BoolArg().parse(str, scheduled))
	return errh->error("syntax error");
    if (scheduled)
	task->reschedule();
    else
	task->unschedule();
    return 0;
}

#if CLICK_DEBUG_SCHEDULING
static String
read_notifier_signal(Element *e, void *thunk)
{
    NotifierSignal *signal = (NotifierSignal *) ((uint8_t *) e + (intptr_t) thunk);
    return signal->unparse(e->router());
}
#endif

#if HAVE_MULTITHREAD
static String
read_task_home_thread(Element *e, void *thunk)
{
    Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
    return String(task->home_thread_id());
}

static int
write_task_home_thread(const String &str, Element *e, void *thunk, ErrorHandler *errh)
{
    Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
    Master *m = task->master();
    int tid;
    if (!IntArg().parse(str, tid) || tid > m->nthreads())
	return errh->error("bad thread");
    task->move_thread(tid);
    return 0;
}
#endif

/** @brief Register handlers for a task.
 *
 * @param task Task object
 * @param signal optional NotifierSignal object
 * @param flags defines handlers to install
 * @param prefix prefix for each handler
 *
 * Adds a standard set of handlers for the task.  They can include:
 *
 * @li A "scheduled" read handler, which returns @c true if the task is
 * scheduled and @c false if not.
 * @li A "scheduled" write handler, which accepts a Boolean and unschedules
 * or reschedules the task as appropriate.
 * @li A "tickets" read handler, which returns the task's tickets.
 * @li A "tickets" write handler to set the task's tickets.
 * @li A "home_thread" read handler, which returns the task's home thread ID.
 * @li A "home_thread" write handler, which sets the task's home thread ID.
 *
 * The @a flags argument controls which handlers are installed.  By default,
 * this is all but the the "scheduled" write handler.  Individual flags are:
 *
 * @li TASKHANDLER_WRITE_SCHEDULED: A "scheduled" write handler.
 * @li TASKHANDLER_WRITE_TICKETS: A "tickets" write handler.
 * @li TASKHANDLER_WRITE_HOME_THREAD: A "home_thread" write handler.
 * @li TASKHANDLER_WRITE_ALL: All available write handlers.
 * @li TASKHANDLER_DEFAULT: Equals TASKHANDLER_WRITE_TICKETS |
 * TASKHANDLER_WRITE_HOME_THREAD.
 *
 * Depending on Click's configuration options, some of these handlers might
 * not be available.  If Click was configured with schedule debugging, the
 * "scheduled" read handler will additionally report whether an unscheduled
 * task is pending, and a "notifier" read handler will report the state of the
 * @a signal, if any.
 *
 * Each handler name is prefixed with the @a prefix string, so an element with
 * multiple Task objects can register handlers for each of them.
 *
 * @sa add_read_handler, add_write_handler, set_handler
 */
void
Element::add_task_handlers(Task *task, NotifierSignal *signal, int flags, const String &prefix)
{
    intptr_t task_offset = (uint8_t *)task - (uint8_t *)this;
    void *thunk = (void *)task_offset;
    add_read_handler(prefix + "scheduled", read_task_scheduled, thunk);
    if (flags & TASKHANDLER_WRITE_SCHEDULED)
	add_write_handler(prefix + "scheduled", write_task_scheduled, thunk);
#if HAVE_STRIDE_SCHED
    add_read_handler(prefix + "tickets", read_task_tickets, thunk);
    if (flags & TASKHANDLER_WRITE_TICKETS)
	add_write_handler(prefix + "tickets", write_task_tickets, thunk);
#endif
#if HAVE_MULTITHREAD
    add_read_handler(prefix + "home_thread", read_task_home_thread, thunk);
    if (flags & TASKHANDLER_WRITE_HOME_THREAD)
	add_write_handler(prefix + "home_thread", write_task_home_thread, thunk);
#endif
#if CLICK_DEBUG_SCHEDULING
    if (signal) {
	intptr_t signal_offset = (uint8_t *) signal - (uint8_t *) this;
	add_read_handler(prefix + "notifier", read_notifier_signal, (void *) signal_offset);
    }
#else
    (void) signal;
#endif
}

static int
uint8_t_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    uint8_t *ptr = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    int x;
    if (op == Handler::h_read) {
	str = String((int) *ptr);
	return 0;
    } else if (IntArg().parse(str, x) && x >= 0 && x < 256) {
	*ptr = x;
	return 0;
    } else
	return errh->error("expected uint8_t");
}

static int
bool_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    bool *ptr = reinterpret_cast<bool *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read) {
	str = String(*ptr);
	return 0;
    } else if (BoolArg().parse(str, *ptr))
	return 0;
    else
	return errh->error("expected boolean");
}

static int
uint16_t_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    uint16_t *ptr = reinterpret_cast<uint16_t *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    int x;
    if (op == Handler::h_read) {
	str = String((int) *ptr);
	return 0;
    } else if (IntArg().parse(str, x) && x >= 0 && x < 65536) {
	*ptr = x;
	return 0;
    } else
	return errh->error("expected uint16_t");
}

static int
uint16_t_net_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    uint16_t *ptr = reinterpret_cast<uint16_t *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    int x;
    if (op == Handler::h_read) {
	str = String((int) ntohs(*ptr));
	return 0;
    } else if (IntArg().parse(str, x) && x >= 0 && x < 65536) {
	*ptr = htons(x);
	return 0;
    } else
	return errh->error("expected uint16_t");
}

static int
uint32_t_net_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    uint32_t *ptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    uint32_t x;
    if (op == Handler::h_read) {
	str = String(ntohl(*ptr));
	return 0;
    } else if (IntArg().parse(str, x)) {
	*ptr = htonl(x);
	return 0;
    } else
	return errh->error("expected integer");
}

template <typename T> static int
integer_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    T *ptr = reinterpret_cast<T *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read) {
	str = String(*ptr);
	return 0;
    } else if (IntArg().parse(str, *ptr))
	return 0;
    else
	return errh->error("expected integer");
}

static int
atomic_uint32_t_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    atomic_uint32_t *ptr = reinterpret_cast<atomic_uint32_t *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    uint32_t value;
    if (op == Handler::h_read) {
	str = String(ptr->value());
	return 0;
    } else if (IntArg().parse(str, value)) {
	*ptr = value;
	return 0;
    } else
	return errh->error("expected integer");
}

#if HAVE_FLOAT_TYPES
static int
double_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    double *ptr = reinterpret_cast<double *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read) {
	str = String(*ptr);
	return 0;
    } else if (DoubleArg().parse(str, *ptr))
	return 0;
    else
	return errh->error("expected real number");
}
#endif

static int
string_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *)
{
    String *ptr = reinterpret_cast<String *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read)
	str = *ptr;
    else
	*ptr = str;
    return 0;
}

static int
ip_address_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    IPAddress *ptr = reinterpret_cast<IPAddress *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read) {
	str = ptr->unparse();
	return 0;
    } if (IPAddressArg().parse(str, *ptr, element))
	return 0;
    else
	return errh->error("expected IP address");
}

static int
ether_address_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    EtherAddress *ptr = reinterpret_cast<EtherAddress *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read) {
	str = ptr->unparse();
	return 0;
    } else if (cp_ethernet_address(str, ptr, element))
	return 0;
    else
	return errh->error("expected Ethernet address");
}

static int
timestamp_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    Timestamp *ptr = reinterpret_cast<Timestamp *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read) {
	str = ptr->unparse();
	return 0;
    } else if (cp_time(str, ptr))
	return 0;
    else
	return errh->error("expected timestamp");
}

static int
interval_data_handler(int op, String &str, Element *element, const Handler *h, ErrorHandler *errh)
{
    Timestamp *ptr = reinterpret_cast<Timestamp *>(reinterpret_cast<uintptr_t>(element) + reinterpret_cast<uintptr_t>(h->user_data(op)));
    if (op == Handler::h_read) {
	str = ptr->unparse_interval();
	return 0;
    } else if (cp_time(str, ptr, true))
	return 0;
    else
	return errh->error("expected time in seconds");
}

inline void
Element::add_data_handlers(const char *name, int flags, HandlerCallback callback, void *data)
{
    uintptr_t x = reinterpret_cast<uintptr_t>(data) - reinterpret_cast<uintptr_t>(this);
    set_handler(name, flags, callback, x, x);
}

/** @brief Register read and/or write handlers accessing @a data.
 *
 * @param name handler name
 * @param flags handler flags, containing at least one of Handler::h_read
 * and Handler::h_write
 * @param data pointer to data
 *
 * Registers read and/or write handlers named @a name for this element.  If
 * (@a flags & Handler::h_read), registers a read handler; if (@a flags &
 * Handler::h_write), registers a write handler.  These handlers read or set
 * the data stored at @a *data, which might, for example, be an element
 * instance variable.  This data is unparsed and/or parsed using the expected
 * functions; for example, the <tt>bool</tt> version uses BoolArg::unparse()
 * and BoolArg::parse().  @a name is passed to String::make_stable.  The memory
 * referenced by @a name must remain valid for as long as the router containing
 * this element.
 *
 * Overloaded versions of this function are available for many fundamental
 * data types.
 */
void
Element::add_data_handlers(const char *name, int flags, uint8_t *data)
{
    add_data_handlers(name, flags, uint8_t_data_handler, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, bool *data)
{
    add_data_handlers(name, flags, bool_data_handler, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, uint16_t *data)
{
    add_data_handlers(name, flags, uint16_t_data_handler, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, int *data)
{
    add_data_handlers(name, flags, integer_data_handler<int>, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, unsigned *data)
{
    add_data_handlers(name, flags, integer_data_handler<unsigned>, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, atomic_uint32_t *data)
{
    add_data_handlers(name, flags, atomic_uint32_t_data_handler, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, long *data)
{
    add_data_handlers(name, flags, integer_data_handler<long>, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, unsigned long *data)
{
    add_data_handlers(name, flags, integer_data_handler<unsigned long>, data);
}

#if HAVE_LONG_LONG
/** @overload */
void
Element::add_data_handlers(const char *name, int flags, long long *data)
{
    add_data_handlers(name, flags, integer_data_handler<long long>, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, unsigned long long *data)
{
    add_data_handlers(name, flags, integer_data_handler<unsigned long long>, data);
}
#endif

#if HAVE_FLOAT_TYPES
/** @overload */
void
Element::add_data_handlers(const char *name, int flags, double *data)
{
    add_data_handlers(name, flags, double_data_handler, data);
}
#endif

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, IPAddress *data)
{
    add_data_handlers(name, flags, ip_address_data_handler, data);
}

/** @overload */
void
Element::add_data_handlers(const char *name, int flags, EtherAddress *data)
{
    add_data_handlers(name, flags, ether_address_data_handler, data);
}

/** @brief Register read and/or write handlers accessing @a data.
 *
 * This function's read handler returns *@a data unchanged, and its write
 * handler sets *@a data to the input string as received, without unquoting or
 * removing leading and trailing whitespace.
 */
void
Element::add_data_handlers(const char *name, int flags, String *data)
{
    add_data_handlers(name, flags, string_data_handler, data);
}

/** @brief Register read and/or write handlers accessing @a data.
 * @param name handler name
 * @param flags handler flags, containing at least one of Handler::h_read
 * and Handler::h_write
 * @param data pointer to data
 * @param is_interval If true, the read handler unparses *@a data as an
 *   interval. */
void
Element::add_data_handlers(const char *name, int flags, Timestamp *data,
			   bool is_interval)
{
    if (is_interval)
	add_data_handlers(name, flags, interval_data_handler, data);
    else
	add_data_handlers(name, flags, timestamp_data_handler, data);
}

/** @brief Register read and/or write handlers accessing @a data in network
 * byte order.
 *
 * @param name handler name
 * @param flags handler flags, containing at least one of Handler::h_read
 * and Handler::h_write
 * @param data pointer to data
 *
 * Registers read and/or write handlers named @a name for this element.  If
 * (@a flags & Handler::h_read), registers a read handler; if (@a flags &
 * Handler::h_write), registers a write handler.  These handlers read or set
 * the data stored at @a *data, which might, for example, be an element
 * instance variable.
 */
void
Element::add_net_order_data_handlers(const char *name, int flags, uint16_t *data)
{
    add_data_handlers(name, flags, uint16_t_net_data_handler, data);
}

/** @overload */
void
Element::add_net_order_data_handlers(const char *name, int flags, uint32_t *data)
{
    add_data_handlers(name, flags, uint32_t_net_data_handler, data);
}


static int
configuration_handler(int operation, String &str, Element *e,
		      int argno, const char *keyword, ErrorHandler *errh)
{
    Vector<String> conf;
    cp_argvec(e->configuration(), conf);

    if (keyword && *keyword >= '0' && *keyword <= '9' && keyword[1] == ' ') {
	argno = *keyword - '0';
	keyword += 2;
    }

    bool found = false, found_positional = false;
    String value, rest;
    if (keyword)
	(void) Args(e).bind(conf)
	    .read(keyword, AnyArg(), value).read_status(found)
	    .consume();
    if (!found && argno >= 0 && conf.size() > argno
	&& (!keyword || !cp_keyword(conf[argno], &value, &rest) || !rest))
	found = found_positional = true;

    if (operation == Handler::h_read) {
	if (found_positional)
	    str = conf[argno];
	else if (found)
	    str = value;
	else
	    str = String();
    } else if (keyword || found_positional) {
	if (found_positional)
	    conf[argno] = str;
	else
	    conf.push_back(String(keyword) + " " + str);

	// create new configuration before calling live_reconfigure(), in case
	// it mucks with the 'conf' array
	String new_config = cp_unargvec(conf);

	int r = e->live_reconfigure(conf, errh);
	if (r < 0)
	    return r;
	e->router()->set_econfiguration(e->eindex(), new_config);
    } else
	return errh->error("missing mandatory arguments");

    return 0;
}

/** @brief Standard read handler returning a positional argument.
 *
 * Use this function to define a handler that returns one of an element's
 * positional configuration arguments.  The @a thunk argument is a typecast
 * integer that specifies which one.  For instance, to add "first", "second",
 * and "third" read handlers that return the element's first three
 * configuration arguments:
 *
 * @code
 * add_read_handler("first", read_positional_handler, 0);
 * add_read_handler("second", read_positional_handler, 1);
 * add_read_handler("third", read_positional_handler, 2);
 * @endcode
 *
 * Returns the empty string if there aren't enough arguments.
 *
 * @warning
 * Prefer read_keyword_handler() to read_positional_handler().
 *
 * @sa configuration: used to obtain the element's current configuration.
 * @sa read_keyword_handler, reconfigure_positional_handler, add_read_handler
 */
String
Element::read_positional_handler(Element *element, void *user_data)
{
    String str;
    SilentErrorHandler errh;
    (void) configuration_handler(Handler::h_read, str, element, (uintptr_t) user_data, 0, &errh);
    return str;
}

/** @brief Standard read handler returning a keyword argument.
 *
 * Use this function to define a handler that returns one of an element's
 * keyword configuration arguments.  The @a user_data argument is a C string
 * that specifies which one.  For instance, to add a "data" read handler that
 * returns the element's "DATA" keyword argument:
 *
 * @code
 * add_read_handler("data", read_keyword_handler, "DATA");
 * @endcode
 *
 * Returns the empty string if the configuration doesn't have the specified
 * keyword.
 *
 * The keyword might have been passed as a mandatory positional argument.
 * Click will find it anyway if you prefix the keyword name with the
 * mandatory position.  For example, this tells reconfigure_keyword_handler to
 * use the first positional argument for "DATA" if the keyword itself is
 * missing:
 *
 * @code
 * add_write_handler("data", reconfigure_keyword_handler, "0 DATA");
 * @endcode
 *
 * @sa configuration: used to obtain the element's current configuration.
 * @sa read_positional_handler, reconfigure_keyword_handler, add_read_handler
 */
String
Element::read_keyword_handler(Element *element, void *user_data)
{
    String str;
    SilentErrorHandler errh;
    (void) configuration_handler(Handler::h_read, str, element, -1, (const char *) user_data, &errh);
    return str;
}

/** @brief Standard write handler for reconfiguring an element by changing one
 * of its positional arguments.
 *
 * @warning
 * Prefer reconfigure_keyword_handler() to reconfigure_positional_handler().
 *
 * Use this function to define a handler that, when written, reconfigures an
 * element by changing one of its positional arguments.  The @a user_data
 * argument is a typecast integer that specifies which one.  For typecast
 * integer that specifies which one.  For instance, to add "first", "second",
 * and "third" write handlers that change the element's first three
 * configuration arguments:
 *
 * @code
 * add_write_handler("first", reconfigure_positional_handler, 0);
 * add_write_handler("second", reconfigure_positional_handler, 1);
 * add_write_handler("third", reconfigure_positional_handler, 2);
 * @endcode
 *
 * When one of these handlers is written, Click will call the element's
 * configuration() method to obtain the element's current configuration,
 * change the relevant argument, and call live_reconfigure() to reconfigure
 * the element.
 *
 * @sa configuration: used to obtain the element's current configuration.
 * @sa live_reconfigure: used to reconfigure the element.
 * @sa reconfigure_keyword_handler, read_positional_handler, add_write_handler
 */
int
Element::reconfigure_positional_handler(const String &arg, Element *e,
					void *user_data, ErrorHandler *errh)
{
    String str = arg;
    return configuration_handler(Handler::h_write, str, e, (uintptr_t) user_data, 0, errh);
}

/** @brief Standard write handler for reconfiguring an element by changing one
 * of its keyword arguments.
 *
 * Use this function to define a handler that, when written, reconfigures an
 * element by changing one of its keyword arguments.  The @a thunk argument is
 * a C string that specifies which one.  For typecast integer that specifies
 * which one.  For instance, to add a "data" write handler that changes the
 * element's "DATA" configuration argument:
 *
 * @code
 * add_write_handler("data", reconfigure_keyword_handler, "DATA");
 * @endcode
 *
 * When this handler is written, Click will obtain the element's current
 * configuration, remove any previous occurrences of the keyword, add the new
 * keyword argument to the end, and call live_reconfigure() to reconfigure the
 * element.
 *
 * The keyword might have been passed as a mandatory positional argument.
 * Click will find it anyway if you prefix the keyword name with the
 * mandatory position.  For example, this tells reconfigure_keyword_handler to
 * use the first positional argument for "DATA" if the keyword itself is
 * missing:
 *
 * @code
 * add_write_handler("data", reconfigure_keyword_handler, "0 DATA");
 * @endcode
 *
 * @sa configuration: used to obtain the element's current configuration.
 * @sa live_reconfigure: used to reconfigure the element.
 * @sa reconfigure_positional_handler, read_keyword_handler, add_write_handler
 */
int
Element::reconfigure_keyword_handler(const String &arg, Element *e,
				     void *user_data, ErrorHandler *errh)
{
    String str = arg;
    return configuration_handler(Handler::h_write, str, e, -1, (const char *) user_data, errh);
}

/** @brief Handle a low-level remote procedure call.
 *
 * @param command command number
 * @param[in,out] data pointer to any data for the command
 * @return >= 0 on success, < 0 on failure
 *
 * Low-level RPCs are a lightweight mechanism for communicating between
 * user-level programs and a Click kernel module, although they're also
 * available in user-level Click.  Rather than open a file, write ASCII data
 * to the file, and close it, as for handlers, the user-level program calls @c
 * ioctl() on an open file.  Click intercepts the @c ioctl and calls the
 * llrpc() method, passing it the @c ioctl number and the associated @a data
 * pointer.  The llrpc() method should read and write @a data as appropriate.
 * @a data may be either a kernel pointer (i.e., directly accessible) or a
 * user pointer (i.e., requires special macros to access), depending on the
 * LLRPC number; see <click/llrpc.h> for more.
 *
 * A negative return value is interpreted as an error and returned to the user
 * in @c errno.  Overriding implementations should handle @a commands they
 * understand as appropriate, and call their parents' llrpc() method to handle
 * any other commands.  The default implementation simply returns @c -EINVAL.
 *
 * Click elements should never call each other's llrpc() methods directly; use
 * local_llrpc() instead.
 */
int
Element::llrpc(unsigned command, void *data)
{
    (void) command, (void) data;
    return -EINVAL;
}

/** @brief Execute an LLRPC from within the configuration.
 *
 * @param command command number
 * @param[in,out] data pointer to any data for the command
 *
 * Call this function to execute an element's LLRPC from within another
 * element's code.  It executes any setup code necessary to initialize memory
 * state, then calls llrpc().
 */
int
Element::local_llrpc(unsigned command, void *data)
{
#if CLICK_LINUXMODULE
  mm_segment_t old_fs = get_fs();
  set_fs(get_ds());

  int result = llrpc(command, data);

  set_fs(old_fs);
  return result;
#else
  return llrpc(command, data);
#endif
}

// RUNNING

/** @brief Push packet @a p onto push input @a port.
 *
 * @param port the input port number on which the packet arrives
 * @param p the packet
 *
 * An upstream element transferred packet @a p to this element over a push
 * connection.  This element should process the packet as necessary and
 * return.  The packet arrived on input port @a port.  push() must account for
 * the packet either by pushing it further downstream, by freeing it, or by
 * storing it temporarily.
 *
 * The default implementation calls simple_action().
 */
void
Element::push(int port, Packet *p)
{
    p = simple_action(p);
    if (p)
	output(port).push(p);
}

/** @brief Pull a packet from pull output @a port.
 *
 * @param port the output port number receiving the pull request.
 * @return a packet
 *
 * A downstream element initiated a packet transfer from this element over a
 * pull connection.  This element should return a packet pointer, or null if
 * no packet is available.  The pull request arrived on output port @a port.
 *
 * Often, pull() methods will request packets from upstream using
 * input(i).pull().  The default implementation calls simple_action().
 */
Packet *
Element::pull(int port)
{
    Packet *p = input(port).pull();
    if (p)
	p = simple_action(p);
    return p;
}

/** @brief Process a packet for a simple packet filter.
 *
 * @param p the input packet
 * @return the output packet, or null
 *
 * Many elements act as simple packet filters: they receive a packet from
 * upstream using input 0, process that packet, and forward it downstream
 * using output 0.  The simple_action() method automates this process.  The @a
 * p argument is the input packet.  simple_action() should process the packet
 * and return a packet pointer -- either the same packet, a different packet,
 * or null.  If the return value isn't null, Click will forward that packet
 * downstream.
 *
 * simple_action() must account for @a p, either by returning it, by freeing
 * it, or by emitting it on some alternate push output port.  (An optional
 * second push output port 1 is often used to emit erroneous packets.)
 *
 * simple_action() works equally well for push or pull port pairs.  The
 * default push() method calls simple_action() this way:
 *
 * @code
 * if ((p = simple_action(p)))
 *     output(0).push(p);
 * @endcode
 *
 * The default pull() method calls it this way instead:
 *
 * @code
 * if (Packet *p = input(0).pull())
 *     if ((p = simple_action(p)))
 *         return p;
 * return 0;
 * @endcode
 *
 * An element that implements its processing with simple_action() should have
 * a processing() code like AGNOSTIC or "a/ah", and a flow_code() like
 * COMPLETE_FLOW or "x/x" indicating that packets can flow between the first
 * input and the first output.
 *
 * Most elements that use simple_action() have exactly one input and one
 * output.  However, simple_action() may be used for any number of inputs and
 * outputs; a packet arriving on input port P will be emitted or output port
 * P.
 *
 * For technical branch prediction-related reasons, elements that use
 * simple_action() can perform quite a bit slower than elements that use
 * push() and pull() directly.  The devirtualizer (click-devirtualize) can
 * mitigate this effect.
 */
Packet *
Element::simple_action(Packet *p)
{
    return p;
}

/** @brief Run the element's task.
 *
 * @return true if the task accomplished some meaningful work, false otherwise
 *
 * The Task(Element *) constructor creates a Task object that calls this
 * method when it fires.  Most elements that have tasks use this method.
 *
 * @note The default implementation causes an assertion failure.
 */
bool
Element::run_task(Task *)
{
    assert(0 /* run_task implementation missing */);
    return false;
}

/** @brief Run the element's timer.
 *
 * @param timer the timer object that fired
 *
 * The Timer(Element *) constructor creates a Timer object that calls this
 * method when it fires.  Most elements that have timers use this method.
 *
 * @note The default implementation causes an assertion failure.
 */
void
Element::run_timer(Timer *timer)
{
    assert(0 /* run_timer implementation missing */);
    (void) timer;
}

CLICK_ENDDECLS
