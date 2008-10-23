#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "scopechain.hh"
#include <click/variableenv.hh>
#include <click/straccum.hh>

ElementT *
ScopeChain::push_element(const String &ename)
{
    if (!_routers.size())
	return 0;

    String n(ename), suffix;
    while (1) {
	RouterT *r = _routers.back();
	ElementT *e = r->element(n);
	if (e && !suffix)
	    return e;
	else if (e) {
	    VariableEnvironment *new_scope = new VariableEnvironment(0);
	    ElementClassT *c = e->resolve(*_scopes.back(), new_scope);
	    RouterT *subr = (c ? c->cast_router() : 0);
	    if (subr) {
		_routers.push_back(subr);
		_components.push_back(n);
		_scopes.push_back(new_scope);
		if (ElementT *e = push_element(suffix))
		    return e;
		_routers.pop_back();
		_components.pop_back();
		_scopes.pop_back();
	    }
	    delete new_scope;
	}

	int slash = n.find_right('/');
	if (slash < 0)
	    return 0;
	n = ename.substring(0, slash);
	suffix = ename.substring(slash + 1);
    }
}

void
ScopeChain::enter_element(ElementT *e)
{
    assert(e && e->router() == _routers.back());
    VariableEnvironment *new_scope = new VariableEnvironment(0);
    ElementClassT *c = e->resolve(*_scopes.back(), new_scope);
    assert(c);
    RouterT *subr = c->cast_router();
    assert(subr);
    _routers.push_back(subr);
    _components.push_back(e->name());
    _scopes.push_back(new_scope);
}

void
ScopeChain::pop_element()
{
    assert(_routers.size() > 1);
    delete _scopes.back();
    _routers.pop_back();
    _components.pop_back();
    _scopes.pop_back();
}

String
ScopeChain::flat_name(const String &ename) const
{
    if (!_components.size())
	return ename;
    StringAccum sa;
    for (const String *it = _components.begin();
	 it != _components.end(); ++it)
	sa << *it << '/';
    sa << ename;
    return sa.take_string();
}

ElementClassT *
ScopeChain::resolved_type(ElementT *e) const
{
    assert(e && e->router() == _routers.back());
    return e->resolved_type(*_scopes.back());
}

String
ScopeChain::resolved_config(const String &config) const
{
    if (!config || !_scopes.size())
	return String();
    else
	return cp_expand(config, *_scopes.back());
}
