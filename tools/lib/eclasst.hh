// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ECLASST_HH
#define CLICK_ECLASST_HH
#include <click/string.hh>
#include <stddef.h>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include "etraits.hh"
class RouterT;
class ElementT;
class VariableEnvironment;
class ErrorHandler;
class StringAccum;
class CompoundElementClassT;
class ElementMap;

class ElementClassT { public:

    ElementClassT(const String &);
    virtual ~ElementClassT()		{ }

    static void set_default_class(ElementClassT *);
    static ElementClassT *default_class(const String &);
    static ElementClassT *unused_type();
    static ElementClassT *tunnel_type();

    void use()				{ _use_count++; }
    void unuse()			{ if (--_use_count <= 0) delete this; }

    const String &name() const		{ return _name; }
    const char *name_cc()		{ return _name.cc(); }
    int unique_id() const		{ return _unique_id; }
    int uid() const			{ return _unique_id; }
    static const int UNUSED_UID = -1;
    static const int TUNNEL_UID = 0;

    const ElementTraits &traits() const;
    const String &processing_code() const;
    const String &flow_code() const;
    bool requires(const String &) const;
    bool provides(const String &) const;
    const String &package() const;
    const String &documentation_name() const;
    String documentation_url() const;

    static ElementT *expand_element(ElementT *, RouterT *, const VariableEnvironment &, ErrorHandler *);

    virtual ElementClassT *find_relevant_class(int ninputs, int noutputs, const Vector<String> &);
    virtual void report_signatures(const String &, String, ErrorHandler *);
    virtual ElementT *complex_expand_element(ElementT *, const String &, Vector<String> &, RouterT *, const VariableEnvironment &, ErrorHandler *);
    virtual void collect_primitive_classes(HashMap<String, int> &);

    virtual void unparse_declaration(StringAccum &, const String &);

    virtual bool simple() const			{ return true; }

    virtual void *cast(const char *)		{ return 0; }
    virtual CompoundElementClassT *cast_compound() { return 0; }
    virtual RouterT *cast_router()		{ return 0; }

    static String signature(const String &, int, int, int);
  
  private:

    String _name;
    int _use_count;
    int _unique_id;

    mutable int _traits_version;
    mutable const ElementTraits *_traits;
    
    static ElementClassT *the_unused_type;
    static ElementClassT *the_tunnel_type;
    
    ElementClassT(const String &, int);
    ElementClassT(const ElementClassT &);
    ElementClassT &operator=(const ElementClassT &);

    const ElementTraits &find_traits() const;
    ElementT *direct_expand_element(ElementT *, RouterT *, const VariableEnvironment &, ErrorHandler *);

};

class SynonymElementClassT : public ElementClassT { public:

    SynonymElementClassT(const String &, ElementClassT *);

    ElementClassT *find_relevant_class(int ninputs, int noutputs, const Vector<String> &);
    ElementT *complex_expand_element(ElementT *, const String &, Vector<String> &, RouterT *, const VariableEnvironment &, ErrorHandler *);
    void collect_primitive_classes(HashMap<String, int> &);

    void unparse_declaration(StringAccum &, const String &);

    bool simple() const			{ return false; }
    CompoundElementClassT *cast_compound();
    RouterT *cast_router();
  
  private:

    ElementClassT *_eclass;

};

class CompoundElementClassT : public ElementClassT { public:

    CompoundElementClassT(const String &name, ElementClassT *next_class, int depth, RouterT *enclosing_scope, const String &landmark);
    CompoundElementClassT(const String &name, RouterT *);
    ~CompoundElementClassT();

    int nformals() const		{ return _formals.size(); }
    void add_formal(const String &n)	{ _formals.push_back(n); }
    void finish(ErrorHandler *);
    void check_duplicates_until(ElementClassT *, ErrorHandler *);

    ElementClassT *find_relevant_class(int ninputs, int noutputs, const Vector<String> &);
    void report_signatures(const String &, String, ErrorHandler *);
    ElementT *complex_expand_element(ElementT *, const String &, Vector<String> &, RouterT *, const VariableEnvironment &, ErrorHandler *);
    void collect_primitive_classes(HashMap<String, int> &);

    void unparse_declaration(StringAccum &, const String &);
    
    bool simple() const			{ return false; }
    CompoundElementClassT *cast_compound() { return this; }
    RouterT *cast_router()		{ return _router; }

    String signature() const;

  private:
    
    String _landmark;
    RouterT *_router;
    int _depth;
    Vector<String> _formals;
    int _ninputs;
    int _noutputs;

    ElementClassT *_next;

    bool _circularity_flag;
  
    int actual_expand(RouterT *, int, RouterT *, const VariableEnvironment &, ErrorHandler *);
  
};


extern int32_t default_element_map_version;

inline ElementClassT *
ElementClassT::tunnel_type()
{
    assert(the_tunnel_type);
    return the_tunnel_type;
}

inline ElementClassT *
ElementClassT::unused_type()
{
    assert(the_unused_type);
    return the_unused_type;
}

inline const ElementTraits &
ElementClassT::traits() const
{
    if (_traits_version == default_element_map_version)
	return *_traits;
    else
	return find_traits();
}

inline const String &
ElementClassT::documentation_name() const
{
    return traits().documentation_name;
}

inline const String &
ElementClassT::processing_code() const
{
    return traits().processing_code();
}

inline const String &
ElementClassT::flow_code() const
{
    return traits().flow_code();
}

inline bool
ElementClassT::requires(const String &req) const
{
    return traits().requires(req);
}

inline bool
ElementClassT::provides(const String &req) const
{
    return traits().provides(req);
}

#endif
