// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ECLASST_HH
#define CLICK_ECLASST_HH
#include <click/string.hh>
#include <stddef.h>
#include <click/vector.hh>
#include <click/hashtable.hh>
#include "elementmap.hh"
class ErrorHandler;
class StringAccum;
class RouterT;
class ElementT;
class VariableEnvironment;
class ElementMap;
class SynonymElementClassT;
class LandmarkT;

class ElementClassT { public:

    ElementClassT(const String &name);
    virtual ~ElementClassT();

    static void set_base_type_factory(ElementClassT *(*factory)(const String &));
    static ElementClassT *base_type(const String &);
    static ElementClassT *tunnel_type();

    void use()				{ _use_count++; }
    void unuse()			{ if (--_use_count <= 0) delete this; }

    const String &name() const		{ return _name; }
    String printable_name() const	{ return _printable_name; }
    const char *printable_name_c_str() const { return _printable_name.c_str(); }
    void set_printable_name(const String &s) { _printable_name = s; }
    virtual String landmark() const	{ return String(); }

    // 'primitive' means 'not tunnel, not compound, not synonym'.
    virtual bool primitive() const	{ return true; }
    virtual bool overloaded() const	{ return false; }
    bool tunnel() const			{ return this == tunnel_type(); }

    ElementTraits &force_traits() const;
    ElementTraits &force_traits(ElementMap *emap) const;
    inline const ElementTraits &traits() const;
    inline const ElementTraits &traits(ElementMap *emap) const;
    virtual const ElementTraits *find_traits(ElementMap *emap) const;

    inline const String &port_count_code() const;
    inline const String &processing_code() const;
    inline bool requires(const String &) const;
    inline bool provides(const String &) const;
    const String &package() const;
    const String &documentation_name() const;
    String documentation_url() const;

    // where was this type declared?
    virtual RouterT *declaration_scope() const;
    virtual ElementClassT *overload_type() const;

    virtual void collect_types(HashTable<ElementClassT *, int> &) const;
    virtual void collect_overloads(Vector<ElementClassT *> &) const;

    static ElementT *expand_element(ElementT *element, RouterT *dest,
				    const String &prefix,
				    const VariableEnvironment &env,
				    ErrorHandler *errh);

    virtual bool need_resolve() const;

    /** @brief Resolve an element declaration.
     * @param ninputs number of inputs used
     * @param noutputs number of outputs used
     * @param[in,out] args configuration arguments
     * @param errh error handler
     * @param landmark landmark for errors
     */
    virtual ElementClassT *resolve(int ninputs, int noutputs,
				   Vector<String> &args,
				   ErrorHandler *errh,
				   const LandmarkT &landmark);

    /** @brief Create a new scope with the appropriate declarations.
     * @param args configuration arguments
     * @param scope original scope
     * @param[out] new_scope new scope
     * @pre &scope != &new_scope
     *
     * The new scope equals the old scope, possibly restricted according to
     * lexical scoping, plus declarations for the configuration arguments. */
    virtual void create_scope(const Vector<String> &args,
			      const VariableEnvironment &scope,
			      VariableEnvironment &new_scope);

    virtual ElementT *complex_expand_element(ElementT *element,
					     const Vector<String> &conf,
					     RouterT *dest,
					     const String &prefix,
					     const VariableEnvironment &env,
					     ErrorHandler *errh);

    enum UnparseKind { UNPARSE_NAMED, UNPARSE_ANONYMOUS, UNPARSE_OVERLOAD };
    virtual void unparse_declaration(StringAccum &, const String &, UnparseKind, ElementClassT *stop);
    virtual String unparse_signature() const;
    static String unparse_signature(const String &name, const Vector<String> *formal_types, int nargs, int ninputs, int noutputs);

    virtual void *cast(const char *)		{ return 0; }
    virtual SynonymElementClassT *cast_synonym() { return 0; }
    virtual RouterT *cast_router()		{ return 0; }

  private:

    String _name;
    String _printable_name;
    int _use_count;

    mutable int _traits_version;
    mutable const ElementTraits *_traits;

    static ElementClassT *the_tunnel_type;

    ElementClassT(const ElementClassT &);
    ElementClassT &operator=(const ElementClassT &);

    ElementT *direct_expand_element(ElementT *element, RouterT *dest,
				    const String &prefix,
				    const VariableEnvironment &env,
				    ErrorHandler *errh);

};

class SynonymElementClassT : public ElementClassT { public:

    SynonymElementClassT(const String &, ElementClassT *, RouterT *);

    ElementClassT *synonym_of() const	{ return _eclass; }

    bool need_resolve() const;
    ElementClassT *resolve(int, int, Vector<String> &, ErrorHandler *, const LandmarkT &);
    void create_scope(const Vector<String> &, const VariableEnvironment &, VariableEnvironment &);
    ElementT *complex_expand_element(ElementT *, const Vector<String> &, RouterT *, const String &prefix, const VariableEnvironment &, ErrorHandler *);

    void collect_types(HashTable<ElementClassT *, int> &) const;
    void collect_overloads(Vector<ElementClassT *> &) const;

    void unparse_declaration(StringAccum &, const String &, UnparseKind, ElementClassT *);

    bool primitive() const		{ return false; }
    bool overloaded() const		{ return _eclass->overloaded(); }
    const ElementTraits *find_traits(ElementMap *emap) const;

    RouterT *declaration_scope() const;
    ElementClassT *overload_type() const { return _eclass; }

    SynonymElementClassT *cast_synonym() { return this; }
    RouterT *cast_router();

  private:

    ElementClassT *_eclass;
    RouterT *_declaration_scope;

};


inline ElementClassT *
ElementClassT::tunnel_type()
{
    assert(the_tunnel_type);
    return the_tunnel_type;
}

inline const ElementTraits &
ElementClassT::traits(ElementMap *emap) const
{
    if (_traits_version != emap->version()) {
	_traits = find_traits(emap);
	_traits_version = emap->version();
    }
    return *_traits;
}

inline const ElementTraits &
ElementClassT::traits() const
{
    return traits(ElementMap::default_map());
}

inline ElementTraits &
ElementClassT::force_traits() const
{
    return force_traits(ElementMap::default_map());
}

inline const String &
ElementClassT::documentation_name() const
{
    return traits().documentation_name;
}

inline const String &
ElementClassT::port_count_code() const
{
    return traits().port_count_code;
}

inline const String &
ElementClassT::processing_code() const
{
    return traits().processing_code;
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

template <>
inline size_t hashcode(const ElementClassT * const &e) {
    return CLICK_NAME(hashcode)(static_cast<const void *>(e));
}

#endif
