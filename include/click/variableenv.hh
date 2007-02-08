// -*- c-basic-offset: 4; related-file-name: "../../lib/variableenv.cc" -*-
#ifndef CLICK_VARIABLEENVIRONMENT_HH
#define CLICK_VARIABLEENVIRONMENT_HH
#include <click/string.hh>
#include <click/vector.hh>
CLICK_DECLS
class StringAccum;

class VariableExpander { public:

    VariableExpander()			{ }
    virtual ~VariableExpander()		{ }

    virtual bool expand(const String &var, int vartype, int quote, StringAccum &) = 0;

};

class VariableEnvironment : public VariableExpander { public:
  
    VariableEnvironment()	{ }
    VariableEnvironment(const VariableEnvironment &ve, int depth);

    int depth() const		{ return _depths.size() ? _depths.back() : -1; }
    void enter(const Vector<String> &formals, const Vector<String> &values, int depth);
    bool expand(const String &var, int vartype, int quote, StringAccum &);

  private:

    Vector<String> _formals;
    Vector<String> _values;
    Vector<int> _depths;

};

String cp_expand(const String &, VariableExpander &, bool expand_quote = false);
String cp_expand_in_quotes(const String &, int quote);

CLICK_ENDDECLS
#endif
