// -*- c-basic-offset: 4; related-file-name: "../../../elements/standard/scheduleinfo.cc" -*-
#ifndef CLICK_SCHEDULEINFO_HH
#define CLICK_SCHEDULEINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

ScheduleInfo(ELEMENT PARAM, ...)

=s information

specifies scheduling parameters

=io

None

=d

Provides scheduling parameters for specified elements. Each configuration
argument has the form `ELEMENT PARAM', meaning that the element
named ELEMENT has scheduling parameter PARAM. Scheduling
parameters are real numbers that set how often one element should be
scheduled in relation to another. For example,
if elements A and B have
scheduling parameters 2 and 0.5, respectively, then A will be scheduled
2/0.5 = 4 times as often as B. The default scheduling parameter is 1.

ScheduleInfo elements inside a compound element can specify scheduling
parameters for that compound's components.
Outer ScheduleInfo elements
can specify a ``scheduling parameter'' for the compound
element as a whole. This ``scheduling parameter'' is really a scaling
factor affecting the compound's components. For example, consider this
configuration,

   elementclass Compound {
     i :: InfiniteSource -> output;
     ScheduleInfo(i 0.5);
   }
   c :: Compound -> Discard;
   ScheduleInfo(c 4);

which is the same as the following configuration, after compound elements
are expanded.

   c/i :: InfiniteSource -> Discard@3 :: Discard;
   c/ScheduleInfo@2 :: ScheduleInfo(i 0.5);
   ScheduleInfo@4 :: ScheduleInfo(c 4);

The name of the first ScheduleInfo element starts with `c/', so it is
used to look up scheduling parameters for elements named `c/I<whatever>'.
V<>(This includes all components of the compound element `c'.)
The second ScheduleInfo element, however, has no slash in its name,
so it is used to look up all scheduling parameters,
including scaling factors for compound elements.
The InfiniteSource's final scaling parameter will be 2:
the scaling factor 4 times the local scheduling parameter 0.5.

An outer ScheduleInfo element can override local scheduling parameters.
For example, if the second ScheduleInfo element above was

   ScheduleInfo@4 :: ScheduleInfo(c 4, c/i 10.5)

then the InfiniteSource's final scaling parameter would be 10.5.
*/

class ScheduleInfo : public Element { public:

    enum { FRAC_BITS = 10 };

    ScheduleInfo();

    const char* class_name() const	{ return "ScheduleInfo"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
    int configure(Vector<String>&, ErrorHandler*);

    bool query(const String&, int&) const;
    bool query_prefixes(const String&, int&, String&) const;
    static int query(Element*, ErrorHandler*);
    static void initialize_task(Element*, Task*, bool sched, ErrorHandler*);
    static void initialize_task(Element*, Task*, ErrorHandler*);
    static void join_scheduler(Element*, Task*, ErrorHandler*);

};


inline void
ScheduleInfo::initialize_task(Element* e, Task* t, ErrorHandler* errh)
{
    initialize_task(e, t, true, errh);
}

inline void
ScheduleInfo::join_scheduler(Element* e, Task* t, ErrorHandler* errh)
{
    initialize_task(e, t, true, errh);
}

CLICK_ENDDECLS
#endif
