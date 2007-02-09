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

    int depth() const			{ return _depth; }
    int size() const			{ return _names.size(); }

    const String &name(int i) const	{ return _names[i]; }
    const Vector<String> &values() const	{ return _values; }
    const String &value(int i) const	{ return _values[i]; }
    const String &value(const String &name, bool &found) const;

    void clear()			{ _names.clear(); _values.clear(); }
    
    VariableEnvironment *parent_of(int depth);
    bool define(const String &name, const String &value, bool override);
    bool expand(const String &var, int vartype, int quote, StringAccum &);

  private:

    Vector<String> _names;
    Vector<String> _values;
    int _depth;
    VariableEnvironment *_parent;

};

String cp_expand(const String &, VariableExpander &, bool expand_quote = false);
String cp_expand_in_quotes(const String &, int quote);

CLICK_ENDDECLS
#endif
