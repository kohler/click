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
  
    VariableEnvironment(VariableEnvironment *parent);

    int depth() const		{ return _depth; }
    int size() const		{ return _formals.size(); }

    const String &name(int i) const	{ return _formals[i]; }
    const Vector<String> &values() const	{ return _values; }
    const String &value(int i) const	{ return _values[i]; }
    const String &value(const String &formal, bool &found) const;
    
    VariableEnvironment *parent_of(int depth);
    int define(const String &formal, const String &value);
    bool expand(const String &var, int vartype, int quote, StringAccum &);

  private:

    Vector<String> _formals;
    Vector<String> _values;
    int _depth;
    VariableEnvironment *_parent;

};

String cp_expand(const String &, VariableExpander &, bool expand_quote = false);
String cp_expand_in_quotes(const String &, int quote);

CLICK_ENDDECLS
#endif
