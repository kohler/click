#ifndef SCHEDULEINFO_HH
#define SCHEDULEINFO_HH

/*
 * =c
 * ScheduleInfo(ELEMENT SCHEDULING_PARAM, ...)
 * =io
 * None
 * =d
 * Grab scheduling parameters for specified element. Each configuration
 * argument has the form `ELEMENT SCHEDULING_PARAM', meaning that the element
 * named ELEMENT has the scheduling parameter SCHEDULING_PARAM.
 */

#include "element.hh"

class ScheduleInfo : public Element {

  String _prefix;
  bool _active;
  Vector<String> _element_names;
  Vector<int> _max_tickets;

 public:

  static const int FRAC_BITS = 10;
  static const int DEFAULT = 1<<FRAC_BITS;
  
  ScheduleInfo();
  
  const char *class_name() const		{ return "ScheduleInfo"; }
  
  ScheduleInfo *clone() const			{ return new ScheduleInfo; }
  bool configure_first() const;
  int configure(const String &, ErrorHandler *);

  bool query(const String &, int &) const;
  bool query_prefixes(const String &, int &, String &) const;
  static int query(Element *, ErrorHandler *);
  static void join_scheduler(Element *, ErrorHandler *);
  
};

#endif
