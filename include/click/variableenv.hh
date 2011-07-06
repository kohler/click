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

    virtual int expand(const String &var, String &expansion, int vartype, int depth) const = 0;

};

class VariableEnvironment : public VariableExpander { public:

    VariableEnvironment(VariableEnvironment *parent);

    int depth() const			{ return _depth; }
    int size() const			{ return _names.size(); }

    bool defines(const String &name) const;
    const String &name(int i) const	{ return _names[i]; }
    const Vector<String> &values() const { return _values; }
    const String &value(int i) const	{ return _values[i]; }
    const String &value(const String &name, bool &found) const;

    void clear()			{ _names.clear(); _values.clear(); }

    VariableEnvironment *parent_of(int depth) const;
    bool define(const String &name, const String &value, bool override);
    int expand(const String &var, String &expansion, int vartype, int depth) const;

  private:

    Vector<String> _names;
    Vector<String> _values;
    int _depth;
    VariableEnvironment *_parent;

};

String cp_expand(const String &str, const VariableExpander &env,
		 bool expand_quote = false, int depth = 0);

CLICK_ENDDECLS
#endif
