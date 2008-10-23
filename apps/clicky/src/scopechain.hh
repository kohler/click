#ifndef CLICKY_SCOPECHAIN_HH
#define CLICKY_SCOPECHAIN_HH 1
#include <clicktool/routert.hh>

class ScopeChain { public:

    ScopeChain(RouterT *router) {
	if (router) {
	    _routers.push_back(router);
	    _scopes.push_back(new VariableEnvironment(router->scope()));
	}
    }

    ~ScopeChain() {
	for (VariableEnvironment **it = _scopes.begin();
	     it != _scopes.end(); ++it)
	    delete *it;
    }

    ElementT *push_element(const String &ename);
    void enter_element(ElementT *e);
    void pop_element();

    String flat_name(const String &ename) const;
    ElementClassT *resolved_type(ElementT *e) const;
    String resolved_config(const String &config) const;

    const String &back_component() const {
	return _components.back();
    }
    RouterT *back_router() const {
	return _routers.back();
    }

  private:

    Vector<RouterT *> _routers;
    Vector<String> _components;
    Vector<VariableEnvironment *> _scopes;

    ScopeChain(const ScopeChain &);
    ScopeChain &operator=(const ScopeChain &);

};

#endif
